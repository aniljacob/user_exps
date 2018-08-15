#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "list.h"

#define MAX_NUM_VFS 252

enum pci_dev_type {
	PCI_TYPE_VF,
	PCI_TYPE_PF,
	PCI_TYPE_INVAL
};

enum q_cfg_state{
	Q_INITIAL,
	Q_CONFIGURED,
	Q_INVALID
};

struct free_entry {
	struct list_head list_head;
	u16 next_qbase;
	u16 free;
	u16 index;
};

struct qconf_entry{
	struct list_head list_head;
	//struct xdev *parent;
	u32 idx;
	u16 qbase;
	u16 qmax;
	enum q_cfg_state cfg_state;
	enum pci_dev_type type;
	u8 func_id;
};

struct qconf_entry_head{
	struct list_head vf_list;
	struct list_head vf_free_list;
	struct list_head pf_list;
	u32 vf_qmax;
	u32 pf_qmax;
	u32 vf_qbase;
	u32 qcnt_cfgd_free;
	u32 qcnt_init_free;
	u32 qcnt_init_used;
} qconf_list;

#define pr_info printf
#define pr_warn printf
#define pr_debug printf
#define pr_err printf

static inline void list_insert(struct list_head *new, struct list_head *prev)
{
	new->next = prev;
	new->prev = prev->prev;
	prev->prev->next = new;
	prev->prev = new;
}

static void xdev_dump_qconf(u32 xdev)
{
	struct qconf_entry *_qconf = NULL;
	struct qconf_entry *_tmp = NULL;
	struct list_head *listhead;
	const char *list_str[][8] = {{"vf"},{"pf"},{"vf-free"}};
	int end = 0;

	if (xdev == PCI_TYPE_PF)
		listhead = &qconf_list.pf_list;
	else
		listhead = &qconf_list.vf_list;

	pr_info("Dumping %s list\n", *list_str[xdev]);

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		end = ((int)(_qconf->qbase + _qconf->qmax)) - 1;
		if (end < 0)
			end = 0;
		pr_info("func_id = %u, qmax = %u, qbase = %u~%u, cfg_state = %c\n",
				_qconf->func_id, _qconf->qmax, _qconf->qbase,
				end,
				_qconf->cfg_state ? 'c':'i');
	}

	return;
}

static void xdev_dump_freelist(void)
{
	struct free_entry *_free = NULL;
	struct free_entry *_tmp = NULL;
	struct list_head *listhead = &qconf_list.vf_free_list;

	pr_info("Dumping free list\n");

	list_for_each_entry_safe(_free, _tmp, listhead, list_head) {
		pr_info("free-cnt = %u, qbase-next = %d\n",
				_free->free, _free->next_qbase);
	}

	return;
}

static struct qconf_entry *find_func_qentry(u32 xdev, u8 func_id)
{
	struct qconf_entry *_qconf = NULL;
	struct qconf_entry *_tmp = NULL;
	struct list_head *listhead = &qconf_list.vf_list;

	if (xdev) {
		listhead = &qconf_list.pf_list;
	}

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		if (_qconf->func_id == func_id)
			return _qconf;
	}

	return NULL;
}

struct free_entry *create_free_entry(u16 qmax, u16 qbase)
{
	struct free_entry *_free_entry = NULL;

	_free_entry = (struct free_entry *)calloc(1, sizeof(struct free_entry));
	if (!_free_entry)
		return _free_entry;
	_free_entry->free = qmax;
	_free_entry->next_qbase = qbase;

	return _free_entry;
}

static int init_vf_free_list(void)
{
	struct free_entry *_free_entry = NULL;
	u16 free = 0;
	u16 next_qbase = 0;

	INIT_LIST_HEAD(&qconf_list.vf_free_list);
	free = qconf_list.vf_qmax;
	next_qbase = qconf_list.vf_qbase;

	_free_entry = create_free_entry(free, next_qbase);
	list_add_tail(&_free_entry->list_head, &qconf_list.vf_free_list);

	return 0;
}

static int find_first_fit(u16 qmax, u16 *qbase)
{
	struct free_entry *_free_entry = NULL;
	struct free_entry *_tmp = NULL;
	int err = 0;
	u8 found = 0;

	pr_info("Get first fit %u\n", qmax);
	list_for_each_entry_safe(_free_entry, _tmp, &qconf_list.vf_free_list, list_head) {
		pr_info("%s:%d qbase = %u, qmax = %u\n", __func__, __LINE__, _free_entry->next_qbase, _free_entry->free);
		if (_free_entry->free >= qmax) {
			*qbase = _free_entry->next_qbase;
			_free_entry->free -= qmax;
			_free_entry->next_qbase += qmax; 
			qconf_list.qcnt_cfgd_free -= qmax;
			found = 1;
			pr_info("%s:%d:qbase = %u, qmax = %u\n", __func__, __LINE__, *qbase, _free_entry->free);
			break;
		}
	}

	if (!found) {
		pr_warn("No free slot found free/qmax = %u/%u\n", _free_entry->free ,qmax);
		err = -EINVAL;
	}

	return err;
}

static int defrag_free_list(void)
{
	struct free_entry *_free = NULL;
	struct free_entry *_tmp = NULL;
	struct free_entry *_prev = NULL;
	struct list_head *listhead = &qconf_list.vf_free_list;
	int defrag_cnt = 0;

	pr_debug("Defragmenting free list\n");
	list_for_each_entry_safe(_free, _tmp, listhead, list_head) {
		if (_prev) {
			if ((_prev->next_qbase + _prev->free) == _free->next_qbase) {
				_free->free += _prev->free;
				_free->next_qbase = _prev->next_qbase;
				list_del(&_prev->list_head);
				free(_prev);
				defrag_cnt++;
			}
		}
		_prev = _free;
	}

	if (defrag_cnt)
		pr_debug("Defragmented %d entries\n", defrag_cnt);

	return defrag_cnt;
}


static int update_free_list(struct qconf_entry *entry)
{
	struct free_entry *_free_entry = NULL;
	struct free_entry *_free_new = NULL;
	struct free_entry *_tmp = NULL;
	struct list_head *listhead = &qconf_list.vf_free_list;

	if (entry->cfg_state != Q_CONFIGURED)
		return 0;

	list_for_each_entry_safe(_free_entry, _tmp, listhead, list_head) {
		if (entry->qbase < _free_entry->next_qbase) {
			_free_new = create_free_entry(entry->qmax, entry->qbase);
			list_insert(&_free_new->list_head, &_free_entry->list_head);
			qconf_list.qcnt_cfgd_free += entry->qmax;
			break;
		}
	}

	defrag_free_list();

	return 0;
}

/** grab from initial qconf, make qmax=0, qbase=0 cfg_state as zeroed*/
void grab_from_init_qconf(struct list_head *listhead)
{
	struct qconf_entry *_qconf = NULL;
	struct qconf_entry *_tmp = NULL;
	int grab_cnt = 0;

	if (qconf_list.qcnt_init_used > qconf_list.qcnt_cfgd_free) {
		grab_cnt = qconf_list.qcnt_init_used - qconf_list.qcnt_cfgd_free;
		qconf_list.qcnt_init_used -= grab_cnt;
	}

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		/** already grabbed */
		if (!_qconf->qmax || (_qconf->cfg_state != Q_INITIAL))
			continue;
		if (!grab_cnt--)
			break;
		_qconf->qmax = 0;
		_qconf->qbase = 0;
	}
}

static int xdev_set_qmax(u32 xdev, u8 func_id, u16 qmax)
{	
	struct qconf_entry *func_entry = NULL;
	struct list_head *listhead = &qconf_list.vf_list;
	u32 num_qs_free = qconf_list.qcnt_cfgd_free; 
	int err = 0;
	u16 qbase = 0;

	pr_debug("Setting qmax func_id = %u, qmax = %u\n", func_id, qmax);

	if (xdev)
		listhead = &qconf_list.pf_list; 

	if (qmax > num_qs_free) {
		pr_warn("No free qs!, requested/free = %u/%u\n", qmax, num_qs_free);
		return -EINVAL;
	}

	func_entry = find_func_qentry(xdev, func_id);
	if (!func_entry) {
		pr_err("Set qmax request on non available device %u\n", func_id);
		return -EINVAL;
	}

	if (func_entry->cfg_state == Q_CONFIGURED) {
		update_free_list(func_entry);
	}

	func_entry->qbase = 0;
	err = find_first_fit(qmax, &qbase);
	if (err < 0) {
		if (qmax)
			pr_info("Not able to find a fit for qmax = %u\n", qmax);
		else
			pr_info("0 q size, func_id = %u\n", func_id);

		return err;
	}

	pr_info("%s:%d qbase = %u\n", __func__, __LINE__, qbase);
	func_entry->cfg_state = Q_CONFIGURED;
	func_entry->qmax = qmax;
	func_entry->qbase = qbase;
	list_del(&func_entry->list_head);
	list_add_tail(&func_entry->list_head, listhead);
	grab_from_init_qconf(listhead);

	return 0;
}

static int xdev_del_qconf(u32 xdev, u8 func_id)
{
	struct qconf_entry *_qconf = NULL;

	_qconf = find_func_qentry(xdev, func_id);
	if (_qconf) {
		_qconf->cfg_state = Q_INITIAL;
	}

	return 0;
}

//hello
static int xdev_create_qconf(u32 xdev, u8 func_id)
{
	int err = 0;
	struct qconf_entry *_qconf = NULL;
	struct list_head *listhead = &qconf_list.vf_list;

	if (xdev) {
		listhead = &qconf_list.pf_list;
	}

	_qconf = (struct qconf_entry *)calloc(1, sizeof(struct qconf_entry));
	if (!_qconf)
		return -ENOMEM;

	_qconf->func_id = func_id;
	_qconf->cfg_state = Q_INITIAL;
	if (qconf_list.qcnt_cfgd_free){
		_qconf->qmax = 1;
		_qconf->qbase = 2047 - qconf_list.qcnt_init_used;
	}

	qconf_list.qcnt_init_used++;

	list_add_tail(&_qconf->list_head, listhead);

	return err;
}

//bye
static int xdev_destroy_qconf(u32 xdev, u8 func_id)
{
	struct qconf_entry *_qconf = NULL;

	pr_debug("Destroying func_id = %u\n", func_id);
	_qconf = find_func_qentry(xdev, func_id);
	if (_qconf) {
		list_del(&_qconf->list_head);
		update_free_list(_qconf);
		free(_qconf);
	}
	
	return 0;
}

static int xdev_qconf_init(void)
{
	INIT_LIST_HEAD(&qconf_list.vf_list);
	INIT_LIST_HEAD(&qconf_list.pf_list);
	qconf_list.vf_qmax = 252;
	qconf_list.pf_qmax = 2048 - qconf_list.vf_qmax;
	qconf_list.vf_qbase = qconf_list.pf_qmax;
	qconf_list.qcnt_cfgd_free = qconf_list.vf_qmax;
	qconf_list.qcnt_init_free = qconf_list.vf_qmax;
	qconf_list.qcnt_init_used = 0;
	init_vf_free_list();

	return 0;
}

static void xdev_qconf_cleanup(void)
{
	struct qconf_entry *_qconf = NULL;
	struct qconf_entry *_tmp = NULL;
	struct free_entry *_free_entry = NULL;
	struct free_entry *_free_tmp = NULL;
	struct list_head *listhead = &qconf_list.vf_list;

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		list_del(&_qconf->list_head);
		free(_qconf);
	}

	_qconf = NULL;
	listhead = &qconf_list.pf_list;
	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		list_del(&_qconf->list_head);
		free(_qconf);
	}

	listhead = &qconf_list.vf_free_list;
	list_for_each_entry_safe(_free_entry, _free_tmp, listhead, list_head) {
		list_del(&_free_entry->list_head);
		free(_free_entry);
	}

	return ;
}

u8 enable = 1;
#define PRINTVF(enable) do { \
	if (enable) { \
		xdev_dump_freelist(); \
		xdev_dump_qconf(PCI_TYPE_VF); \
		getchar(); \
	} \
}while (0)

static void test_qconf(void)
{
	u8 i;

	for (i = 0; i < 16; i++)
		xdev_create_qconf(PCI_TYPE_VF, i);
	PRINTVF(enable);
	
	xdev_set_qmax(PCI_TYPE_VF, 0, 2048);
	PRINTVF(enable);
	
	xdev_set_qmax(PCI_TYPE_VF, 2, 100);
	PRINTVF(enable);

	xdev_destroy_qconf(PCI_TYPE_VF, 0);
	PRINTVF(enable);
#if 0
	 for (i = 16; i < 60; i++)
		xdev_create_qconf(PCI_TYPE_VF, i);
#endif
	xdev_del_qconf(PCI_TYPE_VF, 12);
	PRINTVF(enable);
	xdev_set_qmax(PCI_TYPE_VF, 4, 70);
	PRINTVF(enable);
#if 0
	 for (i = 16; i < 60; i++)
		xdev_create_qconf(PCI_TYPE_VF, i);
#endif
	xdev_destroy_qconf(PCI_TYPE_VF, 2);
	PRINTVF(enable);
	xdev_set_qmax(PCI_TYPE_VF, 15, 300);
	PRINTVF(enable);
	xdev_destroy_qconf(PCI_TYPE_VF, 4);
	PRINTVF(enable);
	xdev_set_qmax(PCI_TYPE_VF, 8, 22);
	PRINTVF(enable);
	xdev_set_qmax(PCI_TYPE_VF, 8, 220);
	PRINTVF(enable);
}


int main(int argc, char *argv[])
{
	xdev_qconf_init();
	test_qconf();
	xdev_qconf_cleanup();

	return 0;
}
