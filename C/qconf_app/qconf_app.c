#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "list.h"

#define MAX_NUM_VFS 252

enum pci_dev_type {
	PCI_TYPE_VF,
	PCI_TYPE_PF,
	PCI_TYPE_VF_FREE,
	PCI_TYPE_INVAL
};

enum q_cfg_state{
	Q_INITIAL,
	Q_CONFIGURED,
	Q_FREE,
	Q_INVALID
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

static void xdev_dump_qconf(u32 xdev)
{
	struct qconf_entry *_qconf = NULL;
	struct qconf_entry *_tmp = NULL;
	struct list_head *listhead;
	const char *list_str[][8] = {{"vf"},{"pf"},{"vf-free"}};

	if (xdev == PCI_TYPE_PF)
		listhead = &qconf_list.pf_list;
	else if (xdev == PCI_TYPE_VF_FREE)
		listhead = &qconf_list.vf_free_list;
	else
		listhead = &qconf_list.vf_list;

	pr_info("Dumping %s list\n", *list_str[xdev]);

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		if (xdev == PCI_TYPE_VF_FREE)
			pr_info("q_free = %u, qbase = %u~%u\n",
					 _qconf->qmax, _qconf->qbase,
					_qconf->qbase + _qconf->qmax - 1); 
		else
			pr_info("func_id = %u, qmax = %u, qbase = %u~%u, cfg_state = %c\n",
					_qconf->func_id, _qconf->qmax, _qconf->qbase,
					_qconf->qbase + _qconf->qmax - 1, 
					_qconf->cfg_state ? 'c':'i');
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

static inline void list_insert(struct list_head *new, struct list_head *prev)
{
	new->next = prev;
	new->prev = prev->prev;
	prev->prev->next = new;
}

struct qconf_entry *create_free_entry(u16 qmax, u16 qbase)
{
	struct qconf_entry *_free_entry = NULL;

	_free_entry = (struct qconf_entry *)malloc(sizeof(struct qconf_entry));
	if (!_free_entry)
		return _free_entry;
	_free_entry->qmax = qmax;
	_free_entry->qbase = qbase;
	_free_entry->cfg_state = Q_FREE;

	return _free_entry;
}

static int init_vf_free_list(void)
{
	struct qconf_entry *_free_entry = NULL;
	u16 qmax = 0;
	u16 qbase = 0;

	INIT_LIST_HEAD(&qconf_list.vf_free_list);
	qmax = qconf_list.vf_qmax;
	qbase = qconf_list.vf_qbase;

	_free_entry = create_free_entry(qmax, qbase);
	list_add_tail(&_free_entry->list_head, &qconf_list.vf_free_list);

	return 0;
}

static u16 find_first_fit(u16 qmax)
{
	struct qconf_entry *_free_entry = NULL;
	struct qconf_entry *_tmp = NULL;
	u16 qbase = 0;

	pr_warn("Get first fit %u\n", qmax);
	list_for_each_entry_safe(_free_entry, _tmp, &qconf_list.vf_free_list, list_head) {
		pr_info("%s:%d qbase = %u, qmax = %u\n", __func__, __LINE__, _free_entry->qbase, _free_entry->qmax);
		if (_free_entry->qmax > qmax) {
			qbase = _free_entry->qbase;
			_free_entry->qmax -= qmax;
			_free_entry->qbase += qmax; 
			pr_info("%s:%d:qbase = %u, qmax = %u\n", __func__, __LINE__, qbase, _free_entry->qmax);
			break;
		}
	}

	if (!qbase)
		pr_warn("No free slot found _free_entry->qmax/qmax = %u/%u\n", _free_entry->qmax ,qmax);

	return qbase;
}

static int update_free_list(struct qconf_entry *entry)
{
	struct qconf_entry *_free_entry = NULL;
	struct qconf_entry *_free_new = NULL;
	struct qconf_entry *_tmp = NULL;

	pr_info("%s:%d entry->qbase = %u, entry->qmax = %u\n", __func__, __LINE__, entry->qbase, entry->qmax);
	list_for_each_entry_safe(_free_entry, _tmp, &qconf_list.vf_free_list, list_head) {
		pr_info("%s:%d free-entry qstart = %u, free = %u\n", __func__, __LINE__, _free_entry->qbase, _free_entry->qmax);
		if (entry->qbase < _free_entry->qbase)
			break;
	}

	//is it continuous??
	if (_free_entry->qmax == (entry->qbase + entry->qmax)) {
		_free_entry->qbase -= entry->qmax;
		_free_entry->qmax += entry->qmax;
		pr_info("%s:%d free-entry qstart = %u, free = %u\n", __func__, __LINE__, _free_entry->qbase, _free_entry->qmax);
	} else {
		pr_info("%s:%d free-entry qstart = %u, free = %u\n", __func__, __LINE__, entry->qbase, entry->qmax);
		_free_new = create_free_entry(entry->qmax, entry->qbase);
		list_insert(&_free_new->list_head, &_free_entry->list_head);
	}

	return 0;
}

void reset_init_qconf(struct list_head *listhead, int reset_cnt)
{
	struct qconf_entry *_qconf = NULL;
	struct qconf_entry *_tmp = NULL;
	int cnt = reset_cnt;

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		if (!cnt--)
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
	int qbase = 0;

	if (xdev)
		listhead = &qconf_list.pf_list; 

	if (qmax > num_qs_free) {
		pr_warn("No free qs, requested %u, free = %u\n", qmax, num_qs_free);
		return -EINVAL;
	}

	func_entry = find_func_qentry(xdev, func_id);
	if (!func_entry) {
		pr_info("This should not happen\n");
		return -EINVAL;
	}

	func_entry->cfg_state = Q_CONFIGURED;
	func_entry->qmax = qmax;
	func_entry->qbase = 0;
	qbase = find_first_fit(qmax);
	if (qbase < 0) {
		pr_info("Not able to find a fit for qmax = %u\n", qmax);
		return qbase;
	}
	pr_info("%s:qbase = %u\n", __func__, qbase);
	func_entry->qbase = qbase;
	list_del(&func_entry->list_head);
	list_add_tail(&func_entry->list_head, listhead);
	

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

	_qconf = (struct qconf_entry *)malloc(sizeof(struct qconf_entry));
	if (!_qconf)
		return -ENOMEM;

	_qconf->func_id = func_id;
	_qconf->cfg_state = Q_INITIAL;
	_qconf->qmax = 1;
	_qconf->qbase = 2048 - MAX_NUM_VFS + qconf_list.qcnt_init_used;

	qconf_list.qcnt_init_used++;

	list_add_tail(&_qconf->list_head, listhead);

	return err;
}

//bye
static int xdev_destroy_qconf(u32 xdev, u8 func_id)
{
	struct qconf_entry *_qconf = NULL;

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
	qconf_list.vf_qmax = 1000;
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
	struct list_head *listhead = &qconf_list.vf_list;

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		list_del(&_qconf->list_head);
		free(_qconf);
	}

	_qconf = NULL;
	_tmp = NULL;
	listhead = &qconf_list.pf_list;
	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		list_del(&_qconf->list_head);
		free(_qconf);
	}

	return ;
}

static void test_qconf(void)
{
	u8 i;

	for (i = 0; i < 16; i++)
		xdev_create_qconf(PCI_TYPE_VF, i);
	xdev_dump_qconf(PCI_TYPE_VF);
	xdev_dump_qconf(PCI_TYPE_VF_FREE);
	xdev_set_qmax(PCI_TYPE_VF, 0, 400);
	xdev_dump_qconf(PCI_TYPE_VF);
	xdev_dump_qconf(PCI_TYPE_VF_FREE);
	xdev_set_qmax(PCI_TYPE_VF, 2, 100);
	xdev_dump_qconf(PCI_TYPE_VF);
	xdev_dump_qconf(PCI_TYPE_VF_FREE);
	//xdev_destroy_qconf(PCI_TYPE_VF, 0);
	//xdev_dump_qconf(PCI_TYPE_VF_FREE);
	//xdev_dump_qconf(PCI_TYPE_VF);
//	xdev_del_qconf(PCI_TYPE_VF, 12);
	xdev_set_qmax(PCI_TYPE_VF, 4, 70);
	xdev_dump_qconf(PCI_TYPE_VF);
	xdev_destroy_qconf(PCI_TYPE_VF, 2);
	xdev_set_qmax(PCI_TYPE_VF, 15, 300);
	//xdev_destroy_qconf(PCI_TYPE_VF, 4);
	xdev_dump_qconf(PCI_TYPE_VF_FREE);
	xdev_set_qmax(PCI_TYPE_VF, 8, 22);
	xdev_dump_qconf(PCI_TYPE_VF);
#if 0
	xdev_dump_qconf(PCI_TYPE_VF);
	xdev_create_qconf(PCI_TYPE_VF, 12);
	xdev_dump_qconf(PCI_TYPE_VF);
#endif

}


int main(int argc, char *argv[])
{
	xdev_qconf_init();
	test_qconf();
	xdev_qconf_cleanup();

	return 0;
}
