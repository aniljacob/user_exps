#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "list.h"

enum pci_dev_type {
	PCI_TYPE_VF,
	PCI_TYPE_PF,
	PCI_TYPE_INVAL
};

enum q_cfg_state{
	Q_INITIAL,
	Q_CONFIGURED
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
	struct list_head pf_list;
	u32 vf_qmax;
	u32 pf_qmax;
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
	struct list_head *listhead = &qconf_list.vf_list;

	if (xdev) {
		listhead = &qconf_list.pf_list;
	}

	pr_info("Dumping qconf list\n");

	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		pr_info("func_id = %u, qmax =  %u, qbase = %u, cfg_state = %c\n",
				_qconf->func_id, _qconf->qmax, _qconf->qbase, 
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

static inline void list_insert_reverse(struct list_head *new, struct list_head *prev)
{
	new->next = prev;
	new->prev = prev->prev;
	prev->prev->next = new;
}

static inline void list_insert(struct list_head *new, struct list_head *prev)
{
	new->next = prev->next;
	new->prev = prev;
	prev->next = new;
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

static int find_first_fit_add(struct qconf_entry *func_entry, struct list_head *listhead, u16 qmax)
{	
	int qbase = 0;
	int reset_cnt = 0;
	struct qconf_entry *_qconf = NULL;
	struct qconf_entry *_tmp = NULL;
	struct qconf_entry *_prev = NULL;

	if (list_empty(listhead)) {
			list_add_tail(&func_entry->list_head, listhead);
			return 0;
	}

	if (func_entry->cfg_state == Q_CONFIGURED) {
		if (qconf_list.qcnt_cfgd_free < qmax) {
			pr_info("No enough qs available qmax = %u free=%u\n", qmax, qconf_list.qcnt_cfgd_free);
			return -EINVAL;
		}

		list_for_each_entry_safe_reverse(_qconf, _tmp, listhead, list_head) {
			_prev = _qconf;
			if (_qconf->cfg_state != Q_CONFIGURED)
				break;
		}
		pr_info("qbase = %u, qmax = %u, cfg_state = %u, func_id = %u\n", 
				func_entry->qbase, func_entry->qmax,  
				func_entry->cfg_state, func_entry->func_id);
		qconf_list.qcnt_cfgd_free -= func_entry->qmax;
		qconf_list.qcnt_init_free -= func_entry->qmax;
		qconf_list.qcnt_init_used--;
		qbase = 2048 - (qconf_list.vf_qmax - qconf_list.qcnt_cfgd_free);
		pr_info("qbase new = %u prev->func_id = %u\n", qbase, _prev->func_id);
		func_entry->qbase = qbase;
		list_del(&func_entry->list_head);
		list_insert(&func_entry->list_head, &_prev->list_head);
		if (qconf_list.qcnt_init_used > qconf_list.qcnt_init_free) {
			reset_cnt = qconf_list.qcnt_init_used - qconf_list.qcnt_init_free;
			pr_info("Need to clean '%d' entries\n", reset_cnt);
			reset_init_qconf(listhead, reset_cnt);
		}
		return 0;
	} else {
		if (!qconf_list.qcnt_init_free) {
			pr_info("No free queues\n");
			return -EINVAL;
		}

		list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
			if (_qconf->cfg_state != Q_INITIAL)
				break;
			_prev = _qconf;
			qbase++;
		}
		qconf_list.qcnt_init_free -= func_entry->qmax;
		qconf_list.qcnt_init_used += qmax;
		func_entry->qbase = 2048 - qconf_list.vf_qmax + qbase;
		func_entry->qmax = qmax;
		pr_info("func_entry->qbase = %u func_entry->qmax = %u\n", 
				func_entry->qbase, func_entry->qmax);
		list_add(&func_entry->list_head, &_prev->list_head);
		return 0;
	}

	return -EINVAL;
}

static int xdev_set_qmax(u32 xdev, u8 func_id, u16 qmax)
{	
	struct qconf_entry *func_entry = NULL;
	struct list_head *listhead = &qconf_list.vf_list;
	u32 num_qs_free = qconf_list.qcnt_cfgd_free; 
	int ret = 0;

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
	ret = find_first_fit_add(func_entry, listhead, qmax);
	if (ret < 0) {
		pr_info("Not able to find a fit for qmax = %u\n", qmax);
		return ret;
	}
	
#if 0
	list_for_each_entry_safe(_qconf, _tmp, listhead, list_head) {
		entry->list_head
		qbase_next += _qconf->qmax;
	}
#endif

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

	find_first_fit_add(_qconf, listhead, 1);

	return err;
}

//bye
static int xdev_destroy_qconf(u32 xdev, u8 func_id)
{
	struct qconf_entry *_qconf = NULL;

	_qconf = find_func_qentry(xdev, func_id);
	if (_qconf) {
		list_del(&_qconf->list_head);
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
	qconf_list.qcnt_cfgd_free = qconf_list.vf_qmax;
	qconf_list.qcnt_init_free = qconf_list.vf_qmax;

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
	xdev_set_qmax(PCI_TYPE_VF, 0, 800);
	xdev_set_qmax(PCI_TYPE_VF, 2, 100);
	xdev_del_qconf(PCI_TYPE_VF, 12);
	xdev_destroy_qconf(PCI_TYPE_VF, 12);
	xdev_dump_qconf(PCI_TYPE_VF);
	xdev_set_qmax(PCI_TYPE_VF, 4, 70);
	xdev_set_qmax(PCI_TYPE_VF, 15, 8);
	xdev_set_qmax(PCI_TYPE_VF, 8, 22);
	xdev_dump_qconf(PCI_TYPE_VF);
	xdev_create_qconf(PCI_TYPE_VF, 12);
	xdev_dump_qconf(PCI_TYPE_VF);

}


int main(int argc, char *argv[])
{
	xdev_qconf_init();
	test_qconf();
	xdev_qconf_cleanup();

	return 0;
}
