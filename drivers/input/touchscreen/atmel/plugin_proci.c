/*
 * Atmel maXTouch Touchscreen driver Plug in
 *
 * Copyright (C) 2013 Atmel Co.Ltd
 *
 * Author: Pitter Liao <pitter.liao@atmel.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/****************************************************************  
	Pitter Liao add for macro for the global platform
		email:  pitter.liao@atmel.com 
		mobile: 13244776877
-----------------------------------------------------------------*/
#define PLUG_PROCI_VERSION 0x0010
/*----------------------------------------------------------------
0.10
1 support gesture list set
0.7
1 add error handle in wakeup/process

0.6
1 add request lock and error reset
0.5
1 change T92/T93 report algorithm
2 change set and restore order
0.4
1 drift 20  /  allow mutiltouch /
0.3
1 add oppi direction in restore
0.2
fixed some bugs
0.1
1 first version of pi plugin
*/

#include "plug.h"
#include <linux/delay.h>

#define PI_FLAG_RESUME					(1<<0)
#define PI_FLAG_RESETING				(1<<1)
#define PI_FLAG_RESET					(1<<2)
#define PI_FLAG_CAL						(1<<3)

#define PI_FLAG_GLOVE				   (1<<12)
#define PI_FLAG_STYLUS				  (1<<13)
#define PI_FLAG_WAKEUP				  (1<<14)
#define PI_FLAG_GESTURE					(1<<15)

//more than 16 is high mask

#define PI_FLAG_WORKAROUND_HALT			(1<<31)

#define PI_FLAG_MASK_LOW			(0x000f)
#define PI_FLAG_MASK_NORMAL			(0xfff0)
#define PI_FLAG_MASK				(-1)

#define MAKEWORD(a, b)  ((unsigned short)(((unsigned char)(a)) \
	| ((unsigned short)((unsigned char)(b))) << 8))

enum{
	PI_GLOVE = 0,
	PI_STYLUS,
	PI_DWAKE,
	PI_LIST_NUM,
};

//group:
//0: common  1: with area limit 2: dclick  3: gesture
enum{
	P_COMMON = 0,
	P_AREA,
	
	//DWK_DCLICK,
	GES_ENABLE,
	GL_ENABLE,
	STY_ENABLE,

	DIR_OPPISTE,

	OP_SET,
	OP_CLR,
};


static char *pi_cfg_name[PI_LIST_NUM] = {
	"GLOVE",
	"STYLUS",
	"DWAKE",
};

struct ges_tab_element{
	u16 reg;
	u16 instance;
	unsigned long tag;
	//same at gesture_list definition
	//		BIT[8]GES_SWITCH: Mean the object has global switch control
	//		BIT[7]GES_CTRL_EN: Mean this object has enable ctrl
	//		BIT[0:6]: gesture sub name, 
	//				if has sublist, this is charactor name,otherwise this is element name
#define MAX_GES_NAME_LEN 16
	char name[MAX_GES_NAME_LEN]; //then name must occupy 2 byte space or more
};

struct data_obj{
	struct list_head node;
	struct reg_config rcfg;
};

struct gesture_obj{
	struct data_obj data;
	struct list_head sublist;
};

struct pi_observer{
	unsigned long flag;
	struct reg_config *set[PI_LIST_NUM];
	struct reg_config *stack[PI_LIST_NUM];
	u8 *trace_buf; //gesture trace
	
	struct mutex access_mutex;
	void *mem;
};

struct pi_config{
	struct reg_config *reg_cfg[PI_LIST_NUM];
	int num_reg_cfg[PI_LIST_NUM];
	struct list_head gesture_list;
};

static int get_gesture_trace_data(struct plugin_proci *p, u8 symbol, u8 *buf);
static void clr_gesture_trace_buffer(struct plugin_proci *p);
static struct data_obj * get_data_by_element(struct list_head *head, const struct ges_tab_element *elem);

static void plugin_proci_pi_hook_t6(struct plugin_proci *p, u8 status)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;

	if (status & (MXT_T6_STATUS_RESET|MXT_T6_STATUS_CAL)) {
		dev_dbg2(dev, "PI hook T6 0x%x\n", status);
		
		if (status & MXT_T6_STATUS_CAL) {
			set_and_clr_flag(PI_FLAG_CAL,
				0,&obs->flag);
		}
		
		if (status & MXT_T6_STATUS_RESET) {
			set_and_clr_flag(PI_FLAG_RESETING,
				PI_FLAG_MASK_NORMAL,&obs->flag);
		}
	}else{
		if(test_flag(PI_FLAG_RESETING,&obs->flag))
			set_and_clr_flag(PI_FLAG_RESET,
				PI_FLAG_RESETING,&obs->flag);

			dev_dbg2(dev, "PI hook T6 end\n");
		}

	dev_info2(dev, "mxt pi flag=0x%lx\n",
		 obs->flag);
}

/*
static void plugin_cal_pi_hook_t9(struct plugin_proci *p, int id, int x, int y, u8 status)
{
}
*/
static int plugin_proci_pi_hook_t24(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	//struct pi_observer *obs = (struct pi_observer *)p->obs;
	int state = msg[1] & 0xF;
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE,&pl_flag))
		return -EINVAL;
	
	dev_info2(dev, "mxt hook pi t24 0x%x\n",
		state);

	if (state == 0x4) {
		idx = 0;
		clr_gesture_trace_buffer(p);
	}

	return idx;
}

static int plugin_proci_pi_hook_t61(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int idx = -EINVAL;
	int state = msg[1];

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP,&pl_flag))
		return -EINVAL;

	dev_info(dev, "T61 timer %d status 0x%x %d\n", msg[0], state,(state & MXT_T61_ELAPSED));

	if (msg[0] >= 2) {
		if (state & MXT_T61_ELAPSED) {
			//idx = 0;
			clr_gesture_trace_buffer(p);
		}
	}

	return idx;
}

static int plugin_proci_pi_hook_t81(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int idx = -EINVAL;
	int state = msg[1];

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP,&pl_flag))
		return -EINVAL;
	
	dev_info2(dev, "mxt hook pi t81 0x%x\n",
		state);

	state &= 0x1;
	if(state){
		idx = msg[0];  //Instance ID
		dev_info(dev, "T81 id %d index %d range %u %u\n",
			msg[0],
			idx,
			MAKEWORD(msg[2],msg[1]),
			MAKEWORD(msg[4],msg[3]));
		clr_gesture_trace_buffer(p);
	}
	return idx;
}

static int plugin_proci_pi_hook_t92(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int idx = -EINVAL;
	int state = msg[1];

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE,&pl_flag))
		return -EINVAL;
	
	dev_info2(dev, "mxt hook pi t92 0x%x\n",
		state);

	state &= 0x7F;
		
	if (dcfg->t92.rptcode > CHARACTER_ASCII_BEGIN /*ASCII charactor begin*/) {
		if (state >= dcfg->t92.rptcode) {
			if (state & 0x80)
				idx = 0; //STROKE
			else
				idx = state - dcfg->t92.rptcode; //SYMBOL
		dev_info(dev, "T92 key index %d\n",idx);
	}
		clr_gesture_trace_buffer(p);
	}else {
		idx = 0; //new type report method
		clr_gesture_trace_buffer(p);
	}

	return idx;
}

static int plugin_proci_pi_hook_t93(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	//struct pi_observer *obs = (struct pi_observer *)p->obs;
	int state = msg[1] & 0x83;
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE,&pl_flag))
		return -EINVAL;
	
	dev_info2(dev, "mxt hook pi t93 0x%x\n",
		state);

	if (state & 0x2/*0x1*/) {  //540s: 0x1, T serial: 0x2
		idx = 0;
		dev_info(dev, "T93 key index %d\n",idx);
		clr_gesture_trace_buffer(p);
	}

	return idx;
	}

static int plugin_proci_pi_hook_t99(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	//struct pi_observer *obs = (struct pi_observer *)p->obs;
	int state = msg[1] & 0xF;
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE,&pl_flag))
		return -EINVAL;
	
	dev_info2(dev, "mxt hook pi t99 Event %d, Key %d\n",
		state,msg[2]);

	if (state) {
		idx = 0;
		dev_info(dev, "T99 key index %d\n",idx);
		clr_gesture_trace_buffer(p);
	}

	return idx;
}


static int plugin_proci_pi_hook_t115(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct pi_observer *obs = (struct pi_observer *)p->obs;
	struct device *dev = dcfg->dev;

	int state = msg[1];
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE,&pl_flag))
		return -EINVAL;
	
	dev_info2(dev, "mxt hook pi t115 0x%x\n",
		state);

	if (state & 0x80) { //STROKE
		idx = 0;
		dev_info(dev, "T115 key index %d\n",idx);

		get_gesture_trace_data(p, msg[1] & 0xF, obs->trace_buf);
	}

	return idx;
}

static int plugin_proci_pi_hook_t116(void *pi_id, u8 *msg, unsigned long pl_flag)
{
	struct plugin_proci *p = (struct plugin_proci *)pi_id;
	const struct mxt_config *dcfg = p->dcfg;
	struct pi_config *cfg = p->cfg;
	struct pi_observer *obs = (struct pi_observer *)p->obs;
	struct device *dev = dcfg->dev;

	struct data_obj *cont;
	struct ges_tab_element elemment;

	int state = msg[1];
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE,&pl_flag))
		return -EINVAL;
	
	dev_info2(dev, "mxt hook pi t116 0x%x\n",
		state);

	if (!(state & 0x80)) { //SYMBOL
		elemment.reg = MXT_SPT_SYMBOLGESTURECONFIG_T116;
		elemment.tag = state;
		cont = get_data_by_element(&cfg->gesture_list,&elemment);
		if (cont) {
			idx = 0;
			dev_info(dev, "T116 key index %d\n",idx);
			get_gesture_trace_data(p, state, obs->trace_buf);
		}else {
			dev_info(dev, "Invalid T116 msg %d (%02X)\n",state,msg[2]);
		}
	}

	return idx;
}

static int get_gesture_trace_data(struct plugin_proci *p, u8 symbol, u8 * buf)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	int strcnt;
	int ret = -EINVAL;

	int retry = 5;

	if (!buf)
		return ret;
	
	if (p->get_diagnostic_data) {
		strcnt = 0;
		do {
			ret = p->get_diagnostic_data(p->dev, MXT_T6_DEBUG_GESTRUE, 0, 0, 3, buf, 10, 5);
			if (ret == 0) {
				buf[2] &= 0x1F;
				if(strcnt && strcnt == buf[2])
					break;
				strcnt = buf[2];
				dev_info(dev, "gesture trace data %02x %02x %02x \n",buf[0],buf[1],buf[2]);
			}
			msleep(10);
		}while(--retry);

		if(!retry)
			ret = -ENODATA;
		
		if (ret == 0) {
			if (strcnt && strcnt < MAX_GESTURE_TRACE_STROKE) {
				dev_info(dev, "Found gesture trace data num = %d\n",strcnt);
				ret = p->get_diagnostic_data(p->dev, MXT_T6_DEBUG_GESTRUE, 0, 0, (strcnt << 2) + 3, buf, 10, 5);
			}else {
				dev_err(dev, "Invalid gesture trace data num = %d\n",strcnt);
				ret = -EINVAL;
			}
		}
		if (ret == 0) 
			buf[0] = symbol;
		else{
			dev_err(dev, "Read gesture trace failed %d (%02x %02x %02x)\n",ret,buf[0],buf[1],buf[2]);
			memset(buf, 0, 3);
		}
	}

	return ret;
}

static void clr_gesture_trace_buffer(struct plugin_proci *p)
{
	struct pi_observer *obs = (struct pi_observer *)p->obs;

	memset(obs->trace_buf, 0, 3);
}

static struct data_obj * get_data_by_element(struct list_head *head, const struct ges_tab_element *elem)
{
	struct list_head *node;
	struct data_obj *cont;
	
	list_for_each(node,head) {
		cont = container_of(node, struct data_obj, node);
		if(cont->rcfg.reg == elem->reg &&
			SUBNAME_GES(cont->rcfg.tag) == SUBNAME_GES(elem->tag))
			break;
	}

	if(node == head)
		return NULL;

	return cont;
}

static int set_data_by_element(struct list_head *head,const struct ges_tab_element *elem, unsigned long op)
{
	struct list_head *node;
	struct gesture_obj *gobj;
	struct data_obj *cont;
	struct reg_config *r;
	int ret = -ENODATA;

	list_for_each(node,head) {
		cont = container_of(node, struct data_obj, node);
		gobj = container_of(cont, struct gesture_obj, data);

		r = &cont->rcfg;
		if (r->reg == elem->reg &&
			SUBNAME_GES(r->tag) == SUBNAME_GES(elem->tag)) {
			if(test_flag(BIT_MASK(OP_SET), &op) &&
				!test_flag(BIT_MASK(OP_SET), &r->flag))
				set_and_clr_flag(BIT_MASK(OP_SET), BIT_MASK(OP_CLR) | BIT_MASK(GES_ENABLE), &r->flag);

			if(test_flag(BIT_MASK(OP_CLR), &op) &&
				!test_flag(BIT_MASK(OP_CLR), &r->flag))
				set_and_clr_flag(BIT_MASK(OP_CLR), BIT_MASK(OP_SET) | BIT_MASK(GES_ENABLE), &r->flag);
			ret = 0;
			break;
		}
	}

	return ret;
}

static const struct ges_tab_element * get_element_by_data(const struct ges_tab_element *tab, int num, const struct data_obj *cont)
{
	const struct ges_tab_element *elem;
	int i;
	
	for (i = 0; i < num; i++) {
		elem = &tab[i];
		if(cont->rcfg.reg == elem->reg && 
			SUBNAME_GES(cont->rcfg.tag) == SUBNAME_GES(elem->tag))
			return elem;
	}

	return NULL;
}

void pi_debug_show_content(struct plugin_proci *p, const struct reg_config *r, const char *msg)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	dev_info(dev, "pi gesture %s obj %d %c(%d) %c%c%c %c%c (buf %02x,offset %d,len %d,mask 0x%02lx)\n",
		msg ? msg : "",
		r->reg,
		SUBNAME_GES(r->tag) >= CHARACTER_ASCII_BEGIN ? SUBNAME_GES(r->tag):'.',
		SUBNAME_GES(r->tag),
		test_flag(BIT_MASK(GES_ENABLE), &r->flag) ? 'g':'-',
		test_flag(BIT_MASK(OP_SET), &r->flag) ? 's':'-',
		test_flag(BIT_MASK(OP_CLR), &r->flag) ? 'c':'-',
		test_flag(BIT_MASK(GES_CTRL_EN), &r->tag) ? 'w':'r',
		test_flag(BIT_MASK(GES_SWITCH), &r->tag) ? 's':'-',
		r->buf[0],r->offset,r->len,r->mask);
}

static void debug_show_gesture_list(struct plugin_proci *p, struct list_head *pos)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_config *cfg = p->cfg;
	//struct pi_observer *obs = (struct pi_observer *)p->obs;

	struct list_head *ghead,*chead,*gnode,*cnode;
	struct gesture_obj *gobj;
	struct data_obj *cont;
	struct reg_config *r;

	//update gesture register
	ghead = &cfg->gesture_list;
	list_for_each(gnode,ghead) {
		cont = container_of(gnode, struct data_obj, node);
		gobj = container_of(cont, struct gesture_obj, data);

		//gesture register enable
		if (!pos || pos == gnode) {
			r = &cont->rcfg;
			pi_debug_show_content(p, r, NULL);

			chead = &gobj->sublist;
			list_for_each(cnode,chead) {
				cont = container_of(cnode, struct data_obj, node);
				r = &cont->rcfg;
				dev_info(dev, "\t sub gesture offset %d,len %d,mask 0x%02lx\n",
					r->offset,r->len,r->mask);
			}
		}
	}
}

static int plugin_proci_pi_handle_gesture(struct plugin_proci *p ,unsigned long op, bool reset)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_config *cfg = p->cfg;

	struct list_head *ghead,*chead,*gnode,*cnode;
	struct gesture_obj *gobj;
	struct data_obj *cont;
	struct reg_config rcfg, *r;
	unsigned long op2;
	int i;
	int ret = -EBADR;

	dev_dbg2(dev, "pi gesture handle op  %c%c%c (%lx) reset %d\n",
		test_flag(BIT_MASK(GES_ENABLE), &op) ? 'g' : '-',
		test_flag(BIT_MASK(OP_SET), &op) ? 's' : '-',
		test_flag(BIT_MASK(OP_CLR), &op) ? 'c' : '-',
		op,
		reset);

	//update gesture register
	ghead = &cfg->gesture_list;
	list_for_each(gnode,ghead) {

		cont = container_of(gnode, struct data_obj, node);
		gobj = container_of(cont, struct gesture_obj, data);
		r = &cont->rcfg;
		
		if (!test_flag(BIT_MASK(OP_SET), &r->flag) &&
			!test_flag(BIT_MASK(OP_CLR), &r->flag))
			continue;

		memcpy(&rcfg, r, sizeof(struct reg_config));
		chead = &gobj->sublist;

		//gesture register enable
		if (test_flag(BIT_MASK(GES_ENABLE), &op)) {
			if(test_flag(BIT_MASK(GES_CTRL_EN), &r->tag)) {  //has control bit
				op2 = 0;
				if (test_flag(BIT_MASK(OP_SET), &op)) {
					if (test_flag(BIT_MASK(OP_SET), &r->flag) ||
						!test_flag(BIT_MASK(GES_ENABLE), &r->flag))
						op2 = r->flag;
				}else if (test_flag(BIT_MASK(OP_CLR), &op)) {
					if (test_flag(BIT_MASK(OP_SET), &r->flag) ||  //if set at OP_SET, then clear at OP_CLR
						!test_flag(BIT_MASK(GES_ENABLE), &r->flag))
						op2 = op;
				}
				if (op2) {
					if (test_flag(BIT_MASK(OP_CLR), &op2)) {
						for (i = 0; i < rcfg.len; i++)
							rcfg.buf[i] = (~r->buf[i]) & rcfg.mask;
					}
					
					ret = p->set_obj_cfg(p->dev, &rcfg, NULL, 0);
					//pi_debug_show_content(p, r, "handle gesture");
					
					if (ret == 0) {
						if (test_flag(BIT_MASK(OP_SET), &op))
							set_flag(BIT_MASK(GES_ENABLE), &r->flag);
						else if (test_flag(BIT_MASK(OP_CLR), &op)) {
							if (test_flag(BIT_MASK(OP_SET), &r->flag))
								clear_flag(BIT_MASK(GES_ENABLE), &r->flag);
							else
								set_flag(BIT_MASK(GES_ENABLE), &r->flag);
						}
					}else if (ret == -EIO) {
						dev_err(dev, "pi gesture write reg %d off %d len %d failed\n",
							rcfg.reg,rcfg.offset,rcfg.len);
						break;
					}else
						ret = 0;
				}
			}
		}else {
		//gesture sub type enable
			if (!list_empty(chead) && 
				(reset || !test_flag(BIT_MASK(GES_ENABLE), &r->flag))) {
				list_for_each(cnode,chead) {
					cont = container_of(cnode, struct data_obj, node);
					rcfg.offset = cont->rcfg.offset;
					rcfg.len = cont->rcfg.len;
					rcfg.mask = cont->rcfg.mask;
					if (test_flag(BIT_MASK(OP_CLR), &r->flag/*gobj*/)) { //Clear operation
						for (i = 0; i < rcfg.len; i++)
							rcfg.buf[i] = (~r->buf[i]) & rcfg.mask;
					}
					pi_debug_show_content(p, &rcfg, "handle sublist");
					ret = p->set_obj_cfg(p->dev, &rcfg, NULL, 0);
					if (ret == -EIO) {
						dev_err(dev, "pi ctrl write reg %d off %d len %d failed\n",
							rcfg.reg,rcfg.offset,rcfg.len);
						break;
					}else
						ret = 0;
				}

				if (!ret)
					set_flag(BIT_MASK(GES_ENABLE), &r->flag/*gobj*/);
			}
		}
		
		if (ret == -EIO)
			break;
	}

	dev_err(dev, "pi handle ret %d\n",ret);
	return ret;
}


static unsigned long get_pi_flag(int pi, bool enable, unsigned long pl_flag)
{
	unsigned long flag = 0;

	if (pi == PI_GLOVE) {
		//if (test_flag(PL_FUNCTION_FLAG_GLOVE,&pl_flag))
			flag = BIT_MASK(GL_ENABLE);
	}else if (pi == PI_STYLUS) {
		//if (test_flag(PL_FUNCTION_FLAG_STYLUS,&pl_flag))
			flag = BIT_MASK(STY_ENABLE);
	}else if (pi == PI_DWAKE) {
		/*
		if (test_flag(PL_FUNCTION_FLAG_WAKEUP_DCLICK,&pl_flag))
			flag |= BIT_MASK(DWK_DCLICK);
		*/
		if (test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE,&pl_flag))
			flag |= BIT_MASK(GES_ENABLE);
	}

	if (flag) {
		if (enable)
			flag |= BIT_MASK(OP_SET);
		else
			flag |= BIT_MASK(OP_CLR);

		flag |= BIT_MASK(P_COMMON) | BIT_MASK(P_AREA);
	}
	return flag;
}

static int pi_handle_request(struct plugin_proci *p,const struct reg_config *rcfg, int num_reg_cfg, struct reg_config *rset, struct reg_config *save, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = (struct pi_observer *)p->obs;

	int i,reg;
	u8 *stack_buf;
	int ret;

	for (i = 0; i < num_reg_cfg; i++) {

		if (test_flag(PI_FLAG_RESET,&obs->flag))
			break;
	
		if (test_flag(BIT_MASK(DIR_OPPISTE),&flag)) {
			reg = num_reg_cfg - i - 1;
		}else{
			reg = i;
		}

		memcpy(&rset[reg],&rcfg[reg],sizeof(struct reg_config));
		if (save) {
			memcpy(&save[reg],&rcfg[reg],sizeof(struct reg_config));
			stack_buf = save[reg].buf;
		}else {
			stack_buf = NULL;
		}
		ret = p->set_obj_cfg(p->dev, &rset[reg], stack_buf, flag);
		if (ret == -EIO) {
			dev_err(dev, "pi request write reg %d off %d len %d failed\n",
				rset[reg].reg,rset[reg].offset,rset[reg].len);
			return ret;
		}
		if (rset[reg].sleep)
			msleep(rset[reg].sleep);
	}

	return 0;
}

static int plugin_proci_pi_handle_request(struct plugin_proci *p, int pi, bool enable, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;
	const struct reg_config *rcfg;
	struct reg_config *rstack,*rset;
	unsigned long flag;
	int ret;

	if (pi >= PI_LIST_NUM) {
		dev_err(dev, "Invalid pi request %d \n",pi);
		return -EINVAL;
	}

	mutex_lock(&obs->access_mutex);

	flag = get_pi_flag(pi, enable, pl_flag);
	//save register
	if (enable) {
		rcfg = cfg->reg_cfg[pi];
		rset = obs->set[pi];
		rstack = obs->stack[pi]; 

		ret = pi_handle_request(p, rcfg, cfg->num_reg_cfg[pi], rset, rstack, flag);
		if (ret) {
			dev_err(dev, "pi handle request write reg %d offset %d len %d failed\n",
				rset->reg,rset->offset,rset->len);
			mutex_unlock(&obs->access_mutex);
			return ret;
		}
	}else{
	//restore register
		rset = obs->set[pi];
		rstack = obs->stack[pi];
		flag |= BIT_MASK(DIR_OPPISTE);

		ret = pi_handle_request(p, rstack, cfg->num_reg_cfg[pi], rset, /*rstack*/NULL, flag);
		if (ret) {
			dev_err(dev, "pi handle request write reg %d offset %d len %d failed\n",
				rset->reg,rset->offset,rset->len);
			mutex_unlock(&obs->access_mutex);
			return ret;
		}
	}

	dev_dbg2(dev, "pi handle request result %d\n",
		ret);

	mutex_unlock(&obs->access_mutex);
	return ret;
}

static int plugin_proci_pi_wakeup_enable(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct reg_config t7_power =
		{.reg = MXT_GEN_POWERCONFIG_T7,0,
			.offset = 0,.buf = {0x0}, .len = 2, .mask = 0, .flag = BIT_MASK(P_COMMON)};
	int ret = 0;
	
	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP,&pl_flag))
		return ret;

	if (test_flag(PI_FLAG_WORKAROUND_HALT,&obs->flag))
		return ret;

	if (test_flag(PI_FLAG_WAKEUP, &obs->flag)) {//some error occur in suspend, reset chip
		dev_err(dev, "set wakeup enable loop, set reset\n");
		if(p->reset(p->dev) == 0)
			clear_flag(PI_FLAG_WAKEUP, &obs->flag);
		else
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
	}
	if (!test_flag(PI_FLAG_WAKEUP, &obs->flag)) {
		//clr_gesture_trace_buffer(p);
		ret = plugin_proci_pi_handle_gesture(p,BIT_MASK(GES_ENABLE) | BIT_MASK(OP_SET),0);
		if (ret == 0){
			ret = plugin_proci_pi_handle_request(p, PI_DWAKE, true, pl_flag);
			if (ret == 0) {
			set_flag(PI_FLAG_WAKEUP, &obs->flag);
		return -EBUSY;
			}
		}else if(ret == -EBADR)
			ret = 0;

		if (ret) {
			dev_err(dev, "set wakeup enable failed, set reset\n");
			if (p->reset(p->dev) != 0)
				p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
			p->set_obj_cfg(p->dev, &t7_power, NULL, BIT_MASK(P_COMMON));
		}
	}

	return ret;
}

static int plugin_proci_pi_wakeup_disable(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	unsigned long old_flag = obs->flag;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP,&pl_flag))
		return 0;

	if (test_flag(PI_FLAG_WORKAROUND_HALT,&obs->flag))
		return 0;

	if (test_flag(PI_FLAG_WAKEUP, &obs->flag)) {
		if (plugin_proci_pi_handle_gesture(p,BIT_MASK(GES_ENABLE) | BIT_MASK(OP_CLR),0) == 0) {
			if(plugin_proci_pi_handle_request(p, PI_DWAKE, false, pl_flag) == 0)
			clear_flag(PI_FLAG_WAKEUP, &obs->flag);
		}
	}

	if (test_flag(PI_FLAG_WAKEUP, &obs->flag)) {
		dev_err(dev, "set wakeup disable failed, set reset\n");
		if (p->reset(p->dev) == 0) 
			clear_flag(PI_FLAG_WAKEUP, &obs->flag);
		else
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
	}
	if (test_flag(PI_FLAG_WAKEUP, &old_flag))
		return -EBUSY;

	return 0;
}

struct pi_descriptor{
	unsigned long flag_pl;
	unsigned long flag_pi;
	int pi;
};
#if 0
int wake_switch;
int gesture_switch;
#endif
static void plugin_proci_pi_pre_process_messages(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	//struct pi_config *cfg = p->cfg;
	struct pi_observer *obs = p->obs;	
	
	if (test_flag(PI_FLAG_WORKAROUND_HALT,&obs->flag))
		return;

	dev_dbg2(dev, "mxt plugin_proci_pi_pre_process_messages pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);

// can set some globle flag here, e.g:
#if 0
	if(wake_switch)
		p->set_and_clr_flag(p->dev, PL_FUNCTION_FLAG_WAKEUP_DCLICK, 0);
	else
		p->set_and_clr_flag(p->dev, 0, PL_FUNCTION_FLAG_WAKEUP_DCLICK);
	if(gesture_switch)
		p->set_and_clr_flag(p->dev, PL_FUNCTION_FLAG_WAKEUP_GESTURE, 0);
	else

		p->set_and_clr_flag(p->dev, 0, PL_FUNCTION_FLAG_WAKEUP_GESTURE);
#endif
/*
	if (!test_flag(PI_FLAG_UPDATED,&obs->flag)) {
		ghead = &cfg->gesture_list;
		list_for_each(node,&cfg->gesture_list) {
			if(node == ghead)
				break;

			gobj = (struct gesture_obj *)node;
			if (test_flag(BIT_MASK(OP_SET), &gobj->cfg.flag) &&
				test_flag(BIT_MASK(OP_CLR), &gobj->cfg.flag))
				set_flag(BIT_MASK(GES_ENABLE), &gobj->cfg.flag);
		}
		set_flag(PI_FLAG_UPDATED, &obs->flag);
	}
*/
}

static long plugin_proci_pi_post_process_messages(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	bool enable;
	int i;
	int ret;

	struct pi_descriptor dpr[] = {
		{PL_FUNCTION_FLAG_GLOVE,PI_FLAG_GLOVE,PI_GLOVE},
		{PL_FUNCTION_FLAG_STYLUS,PI_FLAG_STYLUS,PI_STYLUS}};

	struct pi_descriptor dpr_ges = {PL_FUNCTION_FLAG_WAKEUP_GESTURE,PI_FLAG_GESTURE};

	if (test_flag(PI_FLAG_WORKAROUND_HALT,&obs->flag))
		return 0;

	if (test_flag(PI_FLAG_RESETING,&obs->flag))
		return 0;

	dev_dbg2(dev, "mxt pi pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);

	for (i = 0; i < ARRAY_SIZE(dpr); i++) {
		if(test_flag(dpr[i].flag_pl,&pl_flag) != test_flag(dpr[i].flag_pi,&obs->flag)){
			if (test_flag(PI_FLAG_RESET,&obs->flag))
				break;
				
			enable = test_flag(dpr[i].flag_pl,&pl_flag);
			ret = plugin_proci_pi_handle_request(p, dpr[i].pi, enable, pl_flag);
			if (ret) {
				dev_err(dev, "set pi %d enable %d failed %d\n", dpr[i].pi, enable, ret);
				if (p->reset(p->dev) == 0)
					clear_flag(dpr[i].flag_pi, &obs->flag);
				else
					p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
			}else{
				if (enable)
					set_flag(dpr[i].flag_pi, &obs->flag);
				else
					clear_flag(dpr[i].flag_pi, &obs->flag);
			}
		}
	}

	if(test_flag(dpr_ges.flag_pl,&pl_flag) != test_flag(dpr_ges.flag_pi,&obs->flag)) {
		plugin_proci_pi_handle_gesture(p, BIT_MASK(GES_ENABLE)|BIT_MASK(OP_CLR), false); //set all gesture control bit disabled
		plugin_proci_pi_handle_gesture(p, 0, test_flag(PI_FLAG_RESET,&obs->flag)); //update all gesture sublist

		if (test_flag(dpr_ges.flag_pl,&pl_flag))
			set_flag(dpr_ges.flag_pi, &obs->flag);
		else
			clear_flag(dpr_ges.flag_pi, &obs->flag);
	}

	clear_flag(PI_FLAG_MASK_LOW,&obs->flag);

	return MAX_SCHEDULE_TIMEOUT;
}

static struct reg_config mxt_glove_cfg[] = {

	{.reg = MXT_PROCI_GLOVEDETECTION_T78,0,
		.offset = 0,.buf = {0x1}, .len = 1, .mask = 0x1, .flag = BIT_MASK(OP_SET), .tag = 0, .sleep = 0},

	//dummy
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},

	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		9,.buf = {16},1,0,BIT_MASK(OP_SET)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		13,.buf = {0x40,0x02},2,0,BIT_MASK(OP_SET)},
	
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
/*
	{MXT_PROCG_NOISESUPPRESSION_T72,0,
		13,{0x0A},1,0,BIT_MASK(GL_ENABLE)},

	{MXT_PROCI_AUXTOUCHCONFIG_T104,0,
		4,{0x08},1,0,BIT_MASK(GL_ENABLE)},

	{MXT_PROCI_AUXTOUCHCONFIG_T104,0,
		9,{0x08},1,0,BIT_MASK(GL_ENABLE)},

	{MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,0,
		0,{0x0D},1,0,BIT_MASK(GL_ENABLE)},

	{MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,0,
		3,{0x0D,0x0},2,0,BIT_MASK(GL_ENABLE)},
		
	{MXT_PROCI_STYLUS_T47,0,
		0,{0x0},1,0x1,BIT_MASK(GL_ENABLE)},
*/
};

static struct reg_config mxt_stylus_cfg[] = {

	{MXT_PROCI_STYLUS_T47,0,
		0,.buf = {0x1},1,0x1,BIT_MASK(STY_ENABLE)},

	//dummy
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
};

static struct reg_config mxt_dwakeup_cfg[] = {

	//dummy
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
		
#if defined(CONFIG_CHIP_540S)
	{MXT_GEN_ACQUISITIONCONFIG_T8,0,
		2,.buf = {0x05,0x05,0x0,0x0,0x0a,0x19,0x00,0x00,0x01,0x01,0x01,0,0x1},13,0,BIT_MASK(P_COMMON)},

	{MXT_PROCI_TOUCHSUPPRESSION_T42,0,
		0,.buf = {0x41},1,0x41,BIT_MASK(OP_SET)},

	{MXT_SPT_CTECONFIG_T46,0,
		2,.buf = {8,20},2,0,BIT_MASK(P_COMMON)},
/*		
	{MXT_PROCI_STYLUS_T47,0,
		0,{0},1,0x1,BIT_MASK(P_COMMON)},
*/
	{MXT_PROCI_SHIELDLESS_T56,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_PROCI_LENSBENDING_T65,0,
		0,.buf = {0},1,0x1,BIT_MASK(P_COMMON)},
		
	{MXT_PROCG_NOISESUPPRESSION_T72,0,
		0,.buf = {0},1,0x1,BIT_MASK(P_COMMON)},

	{MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,0,
		0,.buf = {0},1,0x1,BIT_MASK(P_COMMON)},

	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		0,.buf = {0},1,0x2,BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		6,.buf = {1},1,0,BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		30,.buf = {70,20,40},3,0,BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		39,.buf = {3,1,1},3,0,BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		43,.buf = {20},1,0,BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		53,.buf = {10},1,0,BIT_MASK(P_COMMON)},
/*
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		8,{4,20},2,0,BIT_MASK(DWK_AREA)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,0,
		19,{2,12},2,0,BIT_MASK(DWK_AREA)},
*/
		
	{MXT_PROCI_AUXTOUCHCONFIG_T104,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
/*

	{MXT_PROCI_UNLOCKGESTURE_T81,0,
		0,{0x1},1,0x1,BIT_MASK(GES_ENABLE)},

	{MXT_PROCI_GESTURE_T92,0,
		0,{0x1},1,0x1,BIT_MASK(GES_ENABLE)},

	{MXT_PROCI_TOUCHSEQUENCELOGGER_T93,0,
		0,{0x1},1,0x1,BIT_MASK(DWK_DCLICK)},
*/
	{MXT_GEN_POWERCONFIG_T7,0,
		0,.buf = {0x3c,0x0f,0x04,0x40,0x00},5,0,BIT_MASK(OP_SET)},

#else
		
	{.reg = MXT_SPT_CTECONFIG_T46,
		.offset = 2,.buf = {8,20}, .len = 2, .mask = 0,.flag = BIT_MASK(P_COMMON)},

/*
	{.reg = MXT_PROCI_GESTURE_T92,
		.offset = 0,.buf = {0x3}, .len = 1, .mask = 0x0,.flag = BIT_MASK(DWK_GESTURE)},

	{.reg = MXT_PROCI_TOUCHSEQUENCELOGGER_T93,
		.offset = 0,.buf = {0x0f}, .len = 1, .mask = 0x0,.flag = BIT_MASK(DWK_DCLICK)},
*/

	{.reg = MXT_PROCI_STYLUS_T47,
		.offset = 0,.buf = {0}, .len = 1, .mask = 0x1,.flag = BIT_MASK(P_COMMON)},

	{ .reg = MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,
		.offset = 0,.buf = {0}, .len = 1, .mask = 0x1,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_PROCG_NOISESUPPRESSION_T72,
		.offset = 0,.buf = {0}, .len = 0, .mask = 0x1,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_PROCI_GLOVEDETECTION_T78,
		.offset = 0,.buf = {0}, .len = 1, .mask = 0x1,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_TOUCH_MULTITOUCHSCREEN_T100,
		.offset = 0,.buf = {0}, .len = 1, .mask = 0x2,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_TOUCH_MULTITOUCHSCREEN_T100,
		.offset = 39,.buf = {0x0,0x0}, .len = 2, .mask = 0x2,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_PROCG_NOISESUPSELFCAP_T108,
		.offset = 0,.buf = {0}, .len = 1, .mask = 0x1,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_GEN_POWERCONFIG_T7,
		.offset = 0,.buf = {0x3C,0x0A,0x4,0x40}, .len = 4, .mask = 0,.flag = BIT_MASK(OP_SET)},

	{.reg = MXT_PROCI_AUXTOUCHCONFIG_T104,
		.offset = 2,.buf = {0x28}, .len = 1, .mask = 0,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_PROCI_AUXTOUCHCONFIG_T104,
		.offset = 7,.buf = {0x28}, .len = 1, .mask = 0,.flag = BIT_MASK(P_COMMON)},

	{.reg = MXT_PROCI_TOUCHSUPPRESSION_T42,
		.offset = 1,.buf = {0X0,0x32,0x19,0x80,0x0,0x0,0x0,0x0,0x0,0xFF}, .len = 10, .mask = 0,.flag = BIT_MASK(P_COMMON)},
	{.reg = MXT_SPT_USERDATA_T38,
			.offset = 0,.buf = {0}, .len = 0, .mask = 0x1, .flag = BIT_MASK(OP_SET),  .sleep = 100},
#endif

	//dummy
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,0,
		0,.buf = {0},0,0x1,BIT_MASK(P_COMMON)},
};

static const struct ges_tab_element gesture_element_array[] = {
	{MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24,0,BIT_MASK(GES_CTRL_EN),"TAP24"},
	{MXT_PROCI_GESTURE_T92,0,BIT_MASK(GES_CTRL_EN),"GES92"},
	{MXT_PROCI_TOUCHSEQUENCELOGGER_T93,0, BIT_MASK(GES_CTRL_EN),"TAP"},
	{MXT_PROCI_KEYGESTUREPROCESSOR_T99,0, BIT_MASK(GES_CTRL_EN),"KEY_TAP"},
	{MXT_PROCI_UNLOCKGESTURE_T81,0,UNLOCK_0 | BIT_MASK(GES_CTRL_EN),"UNLOCK0"},
	{MXT_PROCI_UNLOCKGESTURE_T81,1,UNLOCK_1 | BIT_MASK(GES_CTRL_EN),"UNLOCK1"},
	{MXT_PROCI_SYMBOLGESTURE_T115,0,SLIDING_LEFT | BIT_MASK(GES_CTRL_EN)|BIT_MASK(GES_SWITCH),"LEFT"},
	{MXT_PROCI_SYMBOLGESTURE_T115,0,SLIDING_RIGHT | BIT_MASK(GES_CTRL_EN)|BIT_MASK(GES_SWITCH),"RIGH"},
	{MXT_PROCI_SYMBOLGESTURE_T115,0,SLIDING_UP | BIT_MASK(GES_CTRL_EN)|BIT_MASK(GES_SWITCH),"UP"},
	{MXT_PROCI_SYMBOLGESTURE_T115,0,SLIDING_DOWN | BIT_MASK(GES_CTRL_EN)|BIT_MASK(GES_SWITCH),"DOWN"},
	//MXT_SPT_SYMBOLGESTURECONFIG_T116,0,
	{MXT_PROCI_SYMBOLGESTURE_T115,0,SLIDING_AND_CHARACTER | BIT_MASK(GES_CTRL_EN),"S_115_116"},
};

static bool check_t115_t116_enable(struct list_head *head)
{
	struct list_head *node;
	struct gesture_obj *gobj;
	struct data_obj *cont;
	struct reg_config *r;
	bool enable = false;

	//update gesture register
	list_for_each(node,head) {
		cont = container_of(node, struct data_obj, node);
		gobj = container_of(cont, struct gesture_obj, data);

		//gesture register enable
		r = &cont->rcfg;
		if (r->reg == MXT_PROCI_SYMBOLGESTURE_T115 ||
			r->reg == MXT_SPT_SYMBOLGESTURECONFIG_T116) {
			if(test_flag(BIT_MASK(OP_SET),&r->flag) &&
				test_flag(BIT_MASK(GES_SWITCH),&r->tag)) {
				enable = true;
				break;
			}
		}
	}

	return enable;
}

ssize_t plugin_proci_pi_gesture_show(struct plugin_proci *p, char *buf, size_t count)
{
	struct pi_config *cfg = p->cfg;

	struct list_head *head, *node;
	struct data_obj *cont;
	ssize_t offset = 0;
	int val;
		
	const struct ges_tab_element *elem;

	if (!p->init)
		return 0;
  
	head = &cfg->gesture_list;
	list_for_each(node,head) {
		cont = container_of(node, struct data_obj, node);
		val = 0;
		if (test_flag(BIT_MASK(GES_ENABLE), &cont->rcfg.flag))
			val |= (1<<3);
		if (test_flag(BIT_MASK(OP_SET), &cont->rcfg.flag))
			val |= (1<<0);
		if (test_flag(BIT_MASK(OP_CLR), &cont->rcfg.flag))
			val |= (1<<1);

		elem = get_element_by_data(gesture_element_array,ARRAY_SIZE(gesture_element_array),cont);
		if (elem) {
			offset += scnprintf(buf + offset, count, "%s %02x;\n",
				elem->name,val);
		}else {
			offset += scnprintf(buf + offset, count, "%c %02x;\n",
				SUBNAME_GES(cont->rcfg.tag),val);
		}
	}

	return offset;
}

int plugin_proci_pi_gesture_store(struct plugin_proci *p, const char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_config *cfg = p->cfg;
	struct pi_observer *obs = p->obs;

	struct list_head *head,*node;
	struct data_obj *cont;
	int offset,ofs,val,len,i;
	char name[MAX_GES_NAME_LEN];
	unsigned long op;
	bool check_switch = false;
	int ret = 0;

	const struct ges_tab_element *elem;
	struct ges_tab_element element;
		
	if (!p->init)
		return 0;
		
	head = &cfg->gesture_list;
	offset = 0;
	do {
		len = sscanf(buf + offset, "%16s %02x;%n", name, &val, &ofs);
		if (len > 0)
			dev_info(dev, "%s=%x; len %d offset %d count %d\n",name,val,len,offset,(int)count);
		if (len != 2) {
			ret = EINVAL;
			break;
		}
		dev_info(dev, "Get %s=%x; len %d ofs %d count %d\n",name,val,len,ofs,(int)count);
		len = strlen(name);
		op = 0;
		if (val & (1<<0))
			op = BIT_MASK(OP_SET);
		else if(val & (1<<1))
			op = BIT_MASK(OP_CLR);
		else
			dev_err(dev, "Bad element %s %x\n",buf,val);
		
		if (len > 1) {
			for (i = 0; i < ARRAY_SIZE(gesture_element_array); i++) {
				elem = &gesture_element_array[i];
				if (strncmp(name,elem->name,MAX_GES_NAME_LEN) == 0) {
					dev_err(dev, "set element %s %lx\n",name,op);
					if (set_data_by_element(head,elem,op) != 0) {
						dev_err(dev, "Failed process element %s %x\n",buf,val);
						ret = EINVAL;
					}
					if (test_flag(BIT_MASK(GES_SWITCH), &elem->tag))
						check_switch = true;
					break;
				}
			}
		}else if(len == 1){
			list_for_each(node,head) {
				cont = container_of(node, struct data_obj, node);
				if(!test_flag(BIT_MASK(GES_CTRL_EN), &cont->rcfg.tag) &&
					SUBNAME_GES(cont->rcfg.tag) == name[0]) {

					element.reg = cont->rcfg.reg;
					element.tag = cont->rcfg.tag;
					if (set_data_by_element(head, &element , op) != 0) {
						dev_err(dev, "Failed process element %s %x\n",buf,val);
						ret = -EINVAL;
					}
					if (test_flag(BIT_MASK(GES_SWITCH), &element.tag))
						check_switch = true;
					break;
				}
			}
		}
		offset += ofs;
	}while(offset + 1 < count);

	dev_info(dev, "check_switch %d\n",check_switch);

	if (check_switch) {
		if (check_t115_t116_enable(head))
			op = BIT_MASK(OP_SET);
		else
			op = BIT_MASK(OP_CLR);
		element.reg = MXT_PROCI_SYMBOLGESTURE_T115;
		element.tag = SLIDING_AND_CHARACTER;
		if(set_data_by_element(head,&element,op) != 0) {
			dev_err(dev, "Failed process T115 SLIDING_AND_CHARACTER\n");
			ret = -EINVAL;
		}
	}

	clear_flag(PI_FLAG_GESTURE, &obs->flag);

	if(ret || offset < count)
		return -EINVAL;

	return offset;
}

ssize_t plugin_proci_pi_trace_show(struct plugin_proci *p, char *buf, size_t count)
{
	struct pi_observer *obs = p->obs;
	int offset,strcnt;
	u8 *t_buf;
	int i;

	if (!p->init)
		return 0;

	t_buf = obs->trace_buf;
	strcnt = t_buf[2] & 0x1F;
	if (strcnt > MAX_GESTURE_TRACE_STROKE)
		strcnt = MAX_GESTURE_TRACE_STROKE;

	print_hex_dump(KERN_INFO, "[mxt] trace: ", DUMP_PREFIX_NONE, 16, 1,
			t_buf, (strcnt << 2) + 3, false);

	offset = scnprintf(buf, count, "%02hhx,%02hhx;\n", 
		t_buf[0],t_buf[2]);

	t_buf += 3;
	for (i = 0; i < strcnt; i++) {
		offset += scnprintf(buf + offset, count, "%d,%d;\n", 
			MAKEWORD(t_buf[i<<2],t_buf[(i<<2) + 1]),
			MAKEWORD(t_buf[(i<<2) + 2],t_buf[(i<<2) + 3]));
	}
	
	return offset;
}

static int plugin_proci_pi_show(struct plugin_proci *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;
	const struct reg_config *r,*rcfg,*rstack,*rset;
	int i,j;
	
	dev_info(dev, "[mxt]PLUG_PROCI_VERSION: 0x%x\n",PLUG_PROCI_VERSION);

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]status: Flag=0x%08lx\n",
		obs->flag);

	for (i = 0; i < PI_LIST_NUM; i++) {
		rcfg = cfg->reg_cfg[i];
		rstack = obs->stack[i];
		rset = obs->set[i];

		dev_info(dev, "[mxt]PI config %d '%s':\n",i,pi_cfg_name[i]);

		dev_info(dev, "[mxt] rcfg %d\n",cfg->num_reg_cfg[i]);
		r = rcfg;
		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {
			dev_info(dev, "[mxt]config %d-%d: T%d offset %d len %d(%lx,%lx,%hhd): ",i,j, r[j].reg,r[j].offset,r[j].len,r[j].mask,r[j].flag,r[j].sleep);			
			print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
					   r[j].buf, r[j].len, false);
		}
		/*
		dev_info(dev, "[mxt] rset\n");
		r = rset;
		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {
			dev_info(dev, "[mxt]config %d-%d: T%d offset %d len %d(%lx,%lx): ",i,j, r[j].reg,r[j].offset,r[j].len,r[j].mask,r[j].flag);
			print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
					   r[j].buf, r[j].len, false);
		}

		dev_info(dev, "[mxt] rstack\n");
		r = rstack;
		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {
			dev_info(dev, "[mxt]config %d-%d: T%d offset %d len %d(%lx,%lx): ",i,j, r[j].reg,r[j].offset,r[j].len,r[j].mask,r[j].flag);
			print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
					   r[j].buf, r[j].len, false);
		}
		*/
	}

	debug_show_gesture_list(p, NULL);

		
	dev_info(dev, "[mxt]\n");

	return 0;
}

static int plugin_proci_pi_store(struct plugin_proci *p, const char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer * obs = p->obs;
	struct pi_config *cfg = p->cfg;
	int offset,ofs,i,j,k,ret,val;
	struct reg_config rc,*r;
	char name[255],ges;
	struct list_head *ghead;
	struct ges_tab_element element;

	dev_info(dev, "[mxt]pi store:%s\n",buf);

	if (!p->init)
		return 0;

	if (sscanf(buf, "status: Flag=0x%lx\n",
		&obs->flag) > 0) {
		dev_info(dev, "[mxt] OK\n");
	}else{
		if (count > 6 && count < sizeof(name)) {
			ret = sscanf(buf, "%s %n", name, &offset);
			dev_info(dev, "name %s, offset %d, ret %d\n",name,offset,ret);
			if (ret == 1) {
				if (strncmp(name, "config", 6) == 0) {
					ret = sscanf(buf + offset, "%d-%d: %n", &i, &j, &ofs);
					dev_info(dev, "config (%d,%d), offset %d ret %d^\n",i,j,ofs,ret);
		if (ret == 2) {
						offset += ofs;
			if (i >= 0 && i < PI_LIST_NUM) {
				if (j >= 0 && j < cfg->num_reg_cfg[i]) {
					r = cfg->reg_cfg[i];
					memcpy(&rc, &r[j], sizeof(struct reg_config));
					//memcpy(rc.buf, cfg->reg_cfg[i].buf, rc.len);
					ofs = 0;
					ret = sscanf(buf + offset, "T%hd offset %hd len %hd(%lx,%lx,%hhd): %n", 
						&rc.reg, &rc.offset, &rc.len, &rc.mask, &rc.flag, &rc.sleep, &ofs);
					if (ret > 0) {
						dev_info(dev, "%s\n",buf + offset);
						dev_info(dev, "T%hd offset %hd len %hd(%lx,%lx):(ret %d ofs %d)^",rc.reg, rc.offset,rc.len, rc.mask, rc.flag, ret, ofs);
						if (rc.len > MAX_REG_DATA_LEN)
							rc.len = MAX_REG_DATA_LEN;

						for (k = 0; k < rc.len; k++) {
							offset += ofs;
							if (offset < count) {
								dev_info(dev, "%s\n",buf + offset);
								ret = sscanf(buf + offset, "%x %n", 
									&val,&ofs);
								if (ret == 1) {
									rc.buf[k] = (u8)val;
									dev_info(dev, "%x",rc.buf[k]);
								}else
									break;
							}else
								break;
						}
						if (k && ret > 0) {
							print_trunk(rc.buf, 0, k);
							print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
								rc.buf, k, false);
							dev_info(dev, "set buf data %d\n", k);
						}
						
						memcpy(&r[j], &rc, sizeof(struct reg_config));
					}else{
						dev_info(dev, "invalid string: %s\n", buf + offset);
					}   
				}
			}
		}
				}else if (strncmp(name, "gesture", 7) == 0) {
					memset(&element, 0 ,sizeof(element));
					ret = sscanf(buf + offset, "T%hd %n", &element.reg,&ofs);
					if (ret == 1) {
						offset += ofs;
						ret = sscanf(buf + offset, "%c %d", &ges, &val);
						if (ret == 2) {
							ghead = &cfg->gesture_list;
							element.tag = ges;
							ret = set_data_by_element(ghead, &element, val ? BIT_MASK(OP_SET):BIT_MASK(OP_CLR));
							if (ret != 0) {
								dev_err(dev, "Unknow command(T): %s\n",buf);
								return -EINVAL;
							}
						}else {
							dev_err(dev, "Uncompleted command(T%d %d): %s\n",SUBNAME_GES(element.tag),val,buf);
							return -EINVAL;
						}
					}else {
						dev_err(dev, "Unknow command(T): %s\n",buf);
						return -EINVAL;
					}
				} else {
					dev_err(dev, "Unknow command: %s\n",buf);
					return -EINVAL;
				}
			} else{
				dev_err(dev, "Unknow parameter, ret %d\n",ret);
			}
		}
	}
	
	return 0;
}

static struct data_obj * create_new_node(struct list_head *head, int node_size)
{
	struct data_obj *cont;
	struct list_head *n;

	cont = kzalloc(node_size,GFP_KERNEL);
	if (cont) {
		n = &cont->node;
		INIT_LIST_HEAD(n);
		list_add_tail(n,head);
	}
	printk(KERN_INFO "new node %p(%p,%p) head %p(%p,%p)\n",
		cont,cont->node.prev,cont->node.next,
		head,head->prev,head->next);

	return cont;
};

static int init_gesture_list(struct plugin_proci *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_config *cfg = p->cfg;
	struct reg_config rcfg;
	struct gesture_obj *gobj;
	struct data_obj *cont;
	struct ges_tab_element elemment;
	int i,sym_cnt;
	unsigned long sym_tag;
	int ret;

	INIT_LIST_HEAD(&cfg->gesture_list);
	memset(&rcfg, 0, sizeof(rcfg));

	for (i = 0; i < ARRAY_SIZE(gesture_element_array); i++) {
		rcfg.reg = gesture_element_array[i].reg;
		rcfg.instance = gesture_element_array[i].instance;
		rcfg.offset = 0;
		rcfg.len = 1;
		rcfg.mask = 0x2;
		ret = p->get_obj_cfg(p->dev,&rcfg,0);
		if (ret == 0) {
			gobj = (struct gesture_obj *)create_new_node(&cfg->gesture_list,sizeof(struct gesture_obj));
			if (!gobj) {
				dev_err(dev, "Failed to allocate memory for T%d gesture list\n",
					rcfg.reg);
				ret = -ENOMEM;
				goto release;
			}
			memcpy(&gobj->data.rcfg, &rcfg, sizeof(struct reg_config));
			gobj->data.rcfg.tag = gesture_element_array[i].tag;
			gobj->data.rcfg.len = 1;
			gobj->data.rcfg.buf[0] = rcfg.mask;
			gobj->data.rcfg.flag = BIT_MASK(OP_CLR);
			INIT_LIST_HEAD(&gobj->sublist);
		}
	}

//T115
	memset(&elemment, 0, sizeof(elemment));
	elemment.reg = MXT_PROCI_SYMBOLGESTURE_T115;
	for (i = 0; i < SLIDING_NUM; i++) {
		elemment.tag = i;
		cont = get_data_by_element(&cfg->gesture_list,&elemment);
		if (cont) {
			gobj = container_of(cont, struct gesture_obj, data);
			gobj->data.rcfg.offset = 16;
			gobj->data.rcfg.len = 1;
			gobj->data.rcfg.mask = (1<<i);
			gobj->data.rcfg.buf[0] = 0;
		}
	}

//T116
	memset(&rcfg, 0, sizeof(rcfg));
	rcfg.reg = MXT_SPT_SYMBOLGESTURECONFIG_T116;
	rcfg.offset = 1;
	rcfg.len = 4;
	rcfg.mask = 0;
	do {
		sym_cnt = 0;
		ret = p->get_obj_cfg(p->dev,&rcfg,0);
		dev_info(dev, "Read reg %d, offset %d, {%02x %02x %02x %02x} ret %d\n",
			rcfg.reg,rcfg.offset,rcfg.buf[0],rcfg.buf[1],rcfg.buf[2],rcfg.buf[3],ret);
		if (ret == 0) {
				sym_cnt = rcfg.buf[0] & 0xf;
				sym_tag = rcfg.buf[3] & 0x7F;
				if (sym_cnt) {
					if (sym_tag >= CHARACTER_ASCII_BEGIN) {
						memset(&elemment, 0, sizeof(elemment));
						elemment.reg = MXT_SPT_SYMBOLGESTURECONFIG_T116;
						elemment.tag = sym_tag;
						cont = get_data_by_element(&cfg->gesture_list,&elemment);
						if (!cont) {
							gobj = (struct gesture_obj *)create_new_node(&cfg->gesture_list,sizeof(struct gesture_obj));
							if (!gobj) {
								dev_err(dev, "Failed to allocate memory for T%d gesture list\n",
									rcfg.reg);
								ret = -ENOMEM;
								goto release;
							}
							memcpy(&gobj->data.rcfg, &rcfg, sizeof(struct reg_config));
							gobj->data.rcfg.tag = sym_tag | BIT_MASK(GES_SWITCH);
							gobj->data.rcfg.len = 0;
							gobj->data.rcfg.buf[0] = 0;
							gobj->data.rcfg.flag = BIT_MASK(OP_CLR);
							INIT_LIST_HEAD(&gobj->sublist);
						}else {
							gobj = container_of(cont, struct gesture_obj, data);
						}
						cont = create_new_node(&gobj->sublist,sizeof(struct data_obj));
						if (!cont) {
							dev_err(dev, "Failed to allocate memory for T%d ctrl %d list\n",
								rcfg.reg,i);
							ret = -ENOMEM;
							goto release;
						}
						cont->rcfg.offset = rcfg.offset + 3;
						cont->rcfg.len = 1;
						cont->rcfg.mask = 0x80;
						
						gobj->data.rcfg.len++;
						if (gobj->data.rcfg.len >= MAX_GESTURE_SUPPORTED_IN_T116)
							break;
					}
				}else
					break;
		}
		rcfg.offset += (4 + ((sym_cnt + 1)>>1));
	}while(sym_cnt);
	
release:
	return ret;
}

static void deinit_gesture_list(struct plugin_proci *p)
{
	struct pi_config *cfg = p->cfg;

	struct list_head *ghead,*chead,*gnode,*cnode;
	struct gesture_obj *gobj;
	struct data_obj *cont;

	ghead = &cfg->gesture_list;
	list_for_each(gnode,ghead) {
		cont = container_of(gnode, struct data_obj, node);
		gobj = container_of(cont, struct gesture_obj, data);
		list_del(gnode);
		chead = &gobj->sublist;
		list_for_each(cnode,chead) {
			cont = container_of(cnode, struct data_obj, node);
			list_del(cnode);
			kfree(cont);
			cnode = chead;
		}
		kfree(gobj);
		gnode = ghead;
	}
}

static int init_pi_object(struct plugin_proci *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;
	const struct reg_config *rcfg;
	struct reg_config *rstack,*rset;
	int mem_size;

	int i,j,ret;

	cfg->reg_cfg[PI_GLOVE] = mxt_glove_cfg;
	cfg->num_reg_cfg[PI_GLOVE] = ARRAY_SIZE(mxt_glove_cfg);
	cfg->reg_cfg[PI_STYLUS] = mxt_stylus_cfg;
	cfg->num_reg_cfg[PI_STYLUS] = ARRAY_SIZE(mxt_stylus_cfg);
	cfg->reg_cfg[PI_DWAKE] = mxt_dwakeup_cfg;
	cfg->num_reg_cfg[PI_DWAKE] = ARRAY_SIZE(mxt_dwakeup_cfg);

	for (i = 0 , mem_size = 0; i < PI_LIST_NUM; i++) {
		/*
		rcfg = cfg->reg_cfg[i];
		mem_size += cfg->num_reg_cfg[i] * sizeof(struct reg_config);
		for (j = 0; j < cfg->num_reg_cfg[i]; j++)
			mem_size += rcfg[j].len;
		*/
		mem_size += cfg->num_reg_cfg[i] * sizeof(struct reg_config);
	}

	mem_size <<= 1;
	mem_size += MXT_PAGE_SIZE;
	dev_info(dev, "%s: alloc mem %d, each %d\n", 
			__func__,mem_size,(int)sizeof(struct reg_config));
	
	obs->mem = kzalloc(mem_size, GFP_KERNEL);
	if (!obs->mem) {
		dev_err(dev, "Failed to allocate memory for pi observer reg mem\n");
		return -ENOMEM;
	}

	for (i = 0, mem_size = 0; i < PI_LIST_NUM; i++) {
		obs->stack[i] = obs->mem + mem_size;
		mem_size += cfg->num_reg_cfg[i] * sizeof(struct reg_config);
		obs->set[i] = obs->mem + mem_size;
		mem_size += cfg->num_reg_cfg[i] * sizeof(struct reg_config);
		
		rcfg = cfg->reg_cfg[i];
		rstack = obs->stack[i];
		rset = obs->set[i];
		
		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {
		
			memcpy(&rstack[j], &rcfg[j], sizeof(struct reg_config));
			/*
			rstack->buf = obs->mem + mem_size;
			mem_size += rcfg[j].len;

			rset->buf = obs->mem + mem_size;
			mem_size += rcfg[j].len;
			*/
		}
	}

	for (i = 0; i < PI_LIST_NUM; i++) {
		rcfg = cfg->reg_cfg[i];
		rstack = obs->stack[i];
		rset = obs->set[i];
	}

	obs->trace_buf = obs->mem + mem_size;
	mutex_init(&obs->access_mutex);
	ret = init_gesture_list(p);
	if (ret) {
		dev_err(dev, "Failed to call init_gesture_list\n");
	}
	
	return 0;
}

static int deinit_pi_object(struct plugin_proci *p)
{
	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;

	deinit_gesture_list(p);

	memset(cfg->reg_cfg, 0, sizeof(cfg->reg_cfg));
	
	if (obs->mem)
		kfree(obs->mem);
	return 0;
}


static int plugin_proci_pi_init(struct plugin_proci *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	//struct plug_interfwakee *pl = container_of(dcfg, struct plug_interfwakee, init_cfg);
	struct device *dev = dcfg->dev;

	dev_info(dev, "%s: plugin proci pi version 0x%x\n", 
			__func__,PLUG_PROCI_VERSION);

	p->obs = kzalloc(sizeof(struct pi_observer), GFP_KERNEL);
	if (!p->obs) {
		dev_err(dev, "Failed to allocate memory for pi observer\n");
		return -ENOMEM;
	}

	p->cfg = kzalloc(sizeof(struct pi_config), GFP_KERNEL);
	if (!p->cfg) {
		dev_err(dev, "Failed to allocate memory for pi cfg\n");
		kfree(p->obs);
		return -ENOMEM;
	}

	if (init_pi_object(p) != 0) {
		dev_err(dev, "Failed to allocate memory for pi cfg\n");
		kfree(p->obs);
		kfree(p->cfg);
	}
	
	return  0;
}

static void plugin_proci_pi_deinit(struct plugin_proci *p)
{
		deinit_pi_object(p);

	if (p->obs) {
		kfree(p->obs);
	}
	if (p->cfg)
		kfree(p->cfg);
}

struct plugin_proci mxt_plugin_proci_pi = 
{
	.init = plugin_proci_pi_init,
	.deinit = plugin_proci_pi_deinit,
	.start = NULL,
	.stop = NULL,
	.hook_t6 = plugin_proci_pi_hook_t6,
	.hook_t24 = plugin_proci_pi_hook_t24,
	.hook_t61 = plugin_proci_pi_hook_t61,
	.hook_t81 = plugin_proci_pi_hook_t81,
	.hook_t92 = plugin_proci_pi_hook_t92,
	.hook_t93 = plugin_proci_pi_hook_t93,
	.hook_t99 = plugin_proci_pi_hook_t99,
	.hook_t115 = plugin_proci_pi_hook_t115,
	.hook_t116 = plugin_proci_pi_hook_t116,
	.wake_enable = plugin_proci_pi_wakeup_enable,
	.wake_disable = plugin_proci_pi_wakeup_disable,
	.pre_process = plugin_proci_pi_pre_process_messages,
	.post_process = plugin_proci_pi_post_process_messages,
	.show = plugin_proci_pi_show,
	.store = plugin_proci_pi_store,
};

int plugin_proci_init(struct plugin_proci *p)
{
	memcpy(p, &mxt_plugin_proci_pi, sizeof(struct plugin_proci));

	return 0;
}

