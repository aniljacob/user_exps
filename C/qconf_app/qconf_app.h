#ifndef _Q_CONF_H_
#define _Q_CONF_H_

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

int xdev_set_qmax(u32 xdev, u8 func_id, u16 qmax);
int xdev_del_qconf(u32 xdev, u8 func_id);
int xdev_create_qconf(u32 xdev, u8 func_id);
int xdev_destroy_qconf(u32 xdev, u8 func_id);
int xdev_qconf_init(void);
void xdev_qconf_cleanup(void);

#endif
