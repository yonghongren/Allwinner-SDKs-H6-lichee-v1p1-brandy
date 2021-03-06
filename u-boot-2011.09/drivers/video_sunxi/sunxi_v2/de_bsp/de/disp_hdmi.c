/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "disp_hdmi.h"
struct disp_hdmi_private_data {
	u32 enabled;

	disp_tv_mode mode;

	disp_hdmi_func hdmi_func;
	disp_video_timing *video_info;

	disp_clk_info_t hdmi_clk;
	disp_clk_info_t hdmi_ddc_clk;
	disp_clk_info_t lcd_clk;
	disp_clk_info_t drc_clk;

	u32 support_4k;
};

u32 hdmi_used = 0;
u32 hdmi_init_flags = 0;

#if defined(__LINUX_PLAT__)
static spinlock_t hdmi_data_lock;
#endif

static struct disp_hdmi *hdmis = NULL;
static struct disp_hdmi_private_data *hdmi_private = NULL;
s32 disp_hdmi_set_mode(struct disp_hdmi* hdmi, disp_tv_mode mode);
s32 disp_hdmi_enable(struct disp_hdmi* hdmi);

struct disp_hdmi* disp_get_hdmi(u32 screen_id)
{
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();
	if(screen_id >= num_screens) {
		DE_WRN("screen_id %d out of range\n", screen_id);
		return NULL;
	    }

	if(!(bsp_disp_feat_get_supported_output_types(screen_id) & DISP_OUTPUT_TYPE_HDMI)) {
	    DE_WRN("screen_id %d do not support HDMI TYPE!\n", screen_id);
	    return NULL;
	    }

	if(!disp_al_query_hdmi_mod(screen_id)) {
		DE_WRN("hdmi %d is not registered\n", screen_id);
		return NULL;
	}

	return &hdmis[screen_id];
}

struct disp_hdmi_private_data *disp_hdmi_get_priv(struct disp_hdmi *hdmi)
{
	if(NULL == hdmi) {
		DE_WRN("NULL hdl!\n");
		return NULL;
	}

	if(!disp_al_query_hdmi_mod(hdmi->channel_id)) {
		DE_WRN("hdmi %d is not registered\n", hdmi->channel_id);
		return NULL;
	}

	if(!(bsp_disp_feat_get_supported_output_types(hdmi->channel_id) & DISP_OUTPUT_TYPE_HDMI)) {
	    DE_WRN("screen %d do not support HDMI TYPE!\n", hdmi->channel_id);
	    return NULL;
	}

	return &hdmi_private[hdmi->channel_id];
}

//----------------------------
//----hdmi local functions----
//----------------------------
s32 hdmi_clk_init(struct disp_hdmi *hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if(!hdmi || !hdmip) {
	    DE_WRN("hdmi clk init null hdl!\n");
	    return DIS_FAIL;
	}

	if(hdmip->hdmi_clk.clk && hdmip->hdmi_ddc_clk.clk && hdmi_init_flags == 0) {
		hdmi_init_flags = 1;

		hdmip->hdmi_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_clk.clk);
		OSAL_CCMU_SetMclkSrc(hdmip->hdmi_clk.h_clk);
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_clk.h_clk, CLK_ON);
		OSAL_CCMU_CloseMclk(hdmip->hdmi_clk.h_clk);

		hdmip->hdmi_ddc_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_ddc_clk.clk);
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_ddc_clk.h_clk, CLK_ON);
		OSAL_CCMU_CloseMclk(hdmip->hdmi_ddc_clk.h_clk);

#if defined(__LINUX_PLAT__)
		{
			unsigned long flags;
			spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
			hdmip->hdmi_clk.enabled = 1;
			hdmip->hdmi_ddc_clk.enabled = 1;

#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&hdmi_data_lock, flags);
		}
#endif

	}

	return 0;
}

s32 hdmi_clk_exit(struct disp_hdmi *hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if(!hdmi || !hdmip) {
	    DE_WRN("hdmi clk init null hdl!\n");
	    return DIS_FAIL;
	    }

	if(hdmi_init_flags == 1) {
		hdmi_init_flags = 0;

		hdmip->hdmi_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_clk.clk);
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_clk.h_clk, CLK_OFF);
		OSAL_CCMU_CloseMclk(hdmip->hdmi_clk.h_clk);

		hdmip->hdmi_ddc_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_ddc_clk.clk);
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_ddc_clk.h_clk, CLK_OFF);
		OSAL_CCMU_CloseMclk(hdmip->hdmi_ddc_clk.h_clk);

#if defined(__LINUX_PLAT__)
				{
					unsigned long flags;
					spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
					hdmip->hdmi_clk.enabled = 0;
					hdmip->hdmi_ddc_clk.enabled = 0;
#if defined(__LINUX_PLAT__)
					spin_unlock_irqrestore(&hdmi_data_lock, flags);
				}
#endif
	}
	return 0;
}

s32 hdmi_clk_config(struct disp_hdmi *hdmi)
{
//	u32 pll_freq;
//	u32 clk_div;

	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if(!hdmi || !hdmip) {
	    DE_WRN("hdmi clk init null hdl!\n");
	    return DIS_FAIL;
	    }

	//set hdmi clk
	hdmip->hdmi_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_clk.clk);
	if(hdmip->hdmi_clk.enabled == 0) {
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_clk.h_clk, CLK_ON);

#if defined(__LINUX_PLAT__)
		{
		unsigned long flags;
		spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
			hdmip->hdmi_clk.enabled = 1;
#if defined(__LINUX_PLAT__)
		spin_unlock_irqrestore(&hdmi_data_lock, flags);
		}
#endif
	}

	OSAL_CCMU_SetMclkFreq(hdmip->hdmi_clk.h_clk, hdmip->video_info->pixel_clk * (hdmip->video_info->avi_pr + 1));

	OSAL_CCMU_CloseMclk(hdmip->hdmi_clk.h_clk);

	hdmip->hdmi_ddc_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_ddc_clk.clk);
	if(hdmip->hdmi_ddc_clk.enabled == 0) {
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_ddc_clk.h_clk, CLK_ON);

#if defined(__LINUX_PLAT__)
			{
			unsigned long flags;
			spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
				hdmip->hdmi_ddc_clk.enabled = 1;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&hdmi_data_lock, flags);
			}
#endif
	}

	OSAL_CCMU_CloseMclk(hdmip->hdmi_ddc_clk.h_clk);

	//set lcd clk
	hdmip->lcd_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->lcd_clk.clk);
	OSAL_CCMU_SetMclkSrc(hdmip->lcd_clk.h_clk);
	//OSAL_CCMU_SetSrcFreq(hdmip->lcd_clk.clk, 0);
	OSAL_CCMU_SetMclkFreq(hdmip->lcd_clk.h_clk, hdmip->video_info->pixel_clk * (hdmip->video_info->avi_pr + 1));
	OSAL_CCMU_MclkOnOff(hdmip->lcd_clk.h_clk, CLK_ON);
	OSAL_CCMU_CloseMclk(hdmip->lcd_clk.h_clk);
	return 0;
}

s32 hdmi_clk_enable(struct disp_hdmi *hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if(!hdmi || !hdmip) {
	    DE_WRN("hdmi clk init null hdl!\n");
	    return DIS_FAIL;
	}

	hdmi_clk_config(hdmi);

	if(hdmip->drc_clk.clk)
		disp_al_hdmi_clk_enable(hdmip->drc_clk.clk);

	disp_al_hdmi_enable(hdmi->channel_id);

	return 0;
}

s32 hdmi_clk_disable(struct disp_hdmi *hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if(!hdmi || !hdmip) {
	    DE_WRN("hdmi clk init null hdl!\n");
	    return DIS_FAIL;
	}

	disp_al_hdmi_disable(hdmi->channel_id);

	hdmip->hdmi_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_clk.clk);
	if(hdmip->hdmi_clk.enabled == 1) {
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_clk.h_clk, CLK_OFF);

#if defined(__LINUX_PLAT__)
		{
			unsigned long flags;
			spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
				hdmip->hdmi_clk.enabled = 0;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&hdmi_data_lock, flags);
		}
#endif
	}

	OSAL_CCMU_CloseMclk(hdmip->hdmi_clk.h_clk);

	hdmip->hdmi_ddc_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->hdmi_ddc_clk.clk);
	if(hdmip->hdmi_ddc_clk.enabled == 1) {
		OSAL_CCMU_MclkOnOff(hdmip->hdmi_ddc_clk.h_clk, CLK_OFF);

#if defined(__LINUX_PLAT__)
		{
			unsigned long flags;
			spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
				hdmip->hdmi_ddc_clk.enabled = 0;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&hdmi_data_lock, flags);
		}
#endif
	}

	OSAL_CCMU_CloseMclk(hdmip->hdmi_ddc_clk.h_clk);
	hdmip->lcd_clk.h_clk = OSAL_CCMU_OpenMclk(hdmip->lcd_clk.clk);
	OSAL_CCMU_MclkOnOff(hdmip->lcd_clk.h_clk, CLK_OFF);

	OSAL_CCMU_CloseMclk(hdmip->lcd_clk.h_clk);

	if(hdmip->drc_clk.clk)
		disp_al_hdmi_clk_disable(hdmip->drc_clk.clk);

	return 0;
}

//--------------------------------
//----hdmi interface functions----
//--------------------------------

s32 disp_hdmi_set_func(struct disp_hdmi*  hdmi, disp_hdmi_func * func)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
		DE_WRN("hdmi set func null  hdl!\n");
		return DIS_FAIL;
	}

	hdmip->hdmi_func.hdmi_open = func->hdmi_open;
	hdmip->hdmi_func.hdmi_close= func->hdmi_close;
	hdmip->hdmi_func.hdmi_set_mode= func->hdmi_set_mode;
	hdmip->hdmi_func.hdmi_mode_support= func->hdmi_mode_support;
	hdmip->hdmi_func.hdmi_get_HPD_status = func->hdmi_get_HPD_status;
	hdmip->hdmi_func.hdmi_get_input_csc= func->hdmi_get_input_csc;
	hdmip->hdmi_func.hdmi_set_pll = func->hdmi_set_pll;
	hdmip->hdmi_func.hdmi_get_video_timing_info = func->hdmi_get_video_timing_info;
	hdmip->hdmi_func.hdmi_get_video_info_index = func->hdmi_get_video_info_index;

	return 0;
}

s32 disp_hdmi_init(struct disp_hdmi*  hdmi)
{
	s32 ret;
	s32 value = 0;
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if(!hdmi || !hdmip) {
	    DE_WRN("hdmi init null hdl!\n");
	    return DIS_FAIL;
	}

	if(!disp_al_query_hdmi_mod(hdmi->channel_id)) {
		DE_WRN("hdmi %d is not register\n", hdmi->channel_id);
		return DIS_FAIL;
	}

	ret = OSAL_Script_FetchParser_Data("hdmi_para", "hdmi_4k", &value, 1);

	if(ret == 0)
		hdmip->support_4k = value;

	hdmi_clk_init(hdmi);
	disp_al_hdmi_init(hdmi->channel_id, hdmip->support_4k);
	return 0;
}

s32 disp_hdmi_exit(struct disp_hdmi* hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if(!hdmi || !hdmip) {
	    DE_WRN("hdmi init null hdl!\n");
	    return DIS_FAIL;
	}

	if(!disp_al_query_hdmi_mod(hdmi->channel_id)) {
		DE_WRN("hdmi %d is not register\n", hdmi->channel_id);
		return DIS_FAIL;
	}

	disp_al_hdmi_exit(hdmi->channel_id);
	hdmi_clk_exit(hdmi);

    return 0;
}

s32 disp_hdmi_enable(struct disp_hdmi* hdmi)
{
	s32 index, ret;
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
		DE_WRN("hdmi set func null  hdl!\n");
		return DIS_FAIL;
	}

	if(hdmip->hdmi_func.hdmi_get_video_timing_info == NULL) {
		DE_WRN("hdmi_get_video_timing_info func is null\n");
		return DIS_FAIL;
	}

	hdmip->hdmi_func.hdmi_get_video_timing_info(&(hdmip->video_info));

	if(hdmip->video_info == NULL) {
		DE_WRN("video info is null\n");
		return DIS_FAIL;
	}

	if(hdmip->hdmi_func.hdmi_get_video_info_index == NULL) {
		DE_WRN("hdmi_get_video_info_index func is null\n");
		return DIS_FAIL;
	}

	index = hdmip->hdmi_func.hdmi_get_video_info_index(hdmip->mode);

	if(index < 0) {
		DE_WRN("hdmi get video info index fail\n");
		return DIS_FAIL;
	}

	hdmip->video_info = hdmip->video_info + index;

	hdmi_clk_enable(hdmi);
	disp_al_hdmi_init(hdmi->channel_id, hdmip->support_4k);
	disp_al_hdmi_cfg(hdmi->channel_id, hdmip->video_info);
	disp_al_hdmi_enable(hdmi->channel_id);

	if(hdmip->hdmi_func.hdmi_open == NULL)
	    return -1;

	ret = hdmip->hdmi_func.hdmi_open();

#if defined(__LINUX_PLAT__)
	{
	unsigned long flags;
	spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
	hdmip->enabled = 1;
#if defined(__LINUX_PLAT__)
	    spin_unlock_irqrestore(&hdmi_data_lock, flags);
	}
#endif

	return ret;
}

s32 disp_hdmi_disable(struct disp_hdmi* hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
	    DE_WRN("hdmi set func null  hdl!\n");
	    return DIS_FAIL;
	}

	if(hdmip->enabled == 0) {
		DE_WRN("hdmi%d is already closed\n", hdmi->channel_id);
		return DIS_FAIL;
	}

	disp_al_hdmi_disable(hdmi->channel_id);
	hdmi_clk_disable(hdmi);

	if(hdmip->hdmi_func.hdmi_close == NULL)
	    return -1;

	hdmip->hdmi_func.hdmi_close();

#if defined(__LINUX_PLAT__)
	{
	unsigned long flags;
	spin_lock_irqsave(&hdmi_data_lock, flags);
#endif
	hdmip->enabled = 0;
#if defined(__LINUX_PLAT__)
	spin_unlock_irqrestore(&hdmi_data_lock, flags);
	}
#endif

	return 0;
}

s32 disp_hdmi_is_enabled(struct disp_hdmi* hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
	    DE_WRN("hdmi set func null  hdl!\n");
	    return DIS_FAIL;
	}

	if(hdmi->is_enabled)
		return hdmip->enabled;

	return DIS_FAIL;
}


s32 disp_hdmi_set_mode(struct disp_hdmi* hdmi, disp_tv_mode mode)
{
	s32 ret = 0;
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
		DE_WRN("hdmi set func null  hdl!\n");
		return DIS_FAIL;
	}

	if(hdmip->hdmi_func.hdmi_set_mode == NULL) {
		DE_WRN("hdmi mode is null!\n");
		return -1;
	}

	ret = hdmip->hdmi_func.hdmi_set_mode(mode);
	if(ret == 0)
		hdmip->mode = mode;

	return ret;
}

s32 disp_hdmi_get_mode(struct disp_hdmi* hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
		DE_WRN("hdmi set func null  hdl!\n");
		return DIS_FAIL;
	}

	return hdmip->mode;
}

s32 disp_hdmi_get_HPD_status(struct disp_hdmi* hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
		DE_WRN("hdmi set func null  hdl!\n");
		return DIS_FAIL;
	}
	if (hdmip->hdmi_func.hdmi_get_HPD_status)
		return hdmip->hdmi_func.hdmi_get_HPD_status();
	return DIS_FAIL;
}

s32 disp_hdmi_check_support_mode(struct disp_hdmi* hdmi, u8 mode)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
		DE_WRN("hdmi set func null  hdl!\n");
		return DIS_FAIL;
	}

	if(hdmip->hdmi_func.hdmi_mode_support == NULL)
	    return -1;

	return hdmip->hdmi_func.hdmi_mode_support(mode);
}

s32 disp_hdmi_get_input_csc(struct disp_hdmi* hdmi)
{
	struct disp_hdmi_private_data *hdmip = disp_hdmi_get_priv(hdmi);
	if((NULL == hdmi) || (NULL == hdmip)) {
		DE_WRN("hdmi set func null  hdl!\n");
		return DIS_FAIL;
	}

	if(hdmip->hdmi_func.hdmi_get_input_csc == NULL)
	    return -1;

	return hdmip->hdmi_func.hdmi_get_input_csc();
}

s32 disp_init_hdmi(__disp_bsp_init_para * para)
{
	s32 ret;
	s32 value;
	//get sysconfig hdmi_used
	ret = OSAL_Script_FetchParser_Data("hdmi_para", "hdmi_used", &value, 1);
	if(ret == 0)
		hdmi_used = value;

	if(hdmi_used) {
		u32 num_screens;
		u32 screen_id;
		struct disp_hdmi* hdmi;
		struct disp_hdmi_private_data* hdmip;

		DE_INF("disp_init_hdmi\n");
#if defined(__LINUX_PLAT__)
		spin_lock_init(&hdmi_data_lock);
#endif

		num_screens = bsp_disp_feat_get_num_screens();
		hdmis = (struct disp_hdmi *)OSAL_malloc(sizeof(struct disp_hdmi) * num_screens);
		if(NULL == hdmis) {
			DE_WRN("malloc memory fail!\n");
			return DIS_FAIL;
		}

		hdmi_private = (struct disp_hdmi_private_data *)OSAL_malloc(sizeof(struct disp_hdmi_private_data) * num_screens);
		if(NULL == hdmi_private) {
			DE_WRN("malloc memory fail!\n");
			return DIS_FAIL;
		}

		for(screen_id=0; screen_id<num_screens; screen_id++) {
			hdmi = &hdmis[screen_id];
			hdmip = &hdmi_private[screen_id];

			if(!disp_al_query_hdmi_mod(screen_id)) {
				DE_WRN("hdmi mod %d is not registered\n", screen_id);
				continue;
			}

			if(!(bsp_disp_feat_get_supported_output_types(screen_id) & DISP_OUTPUT_TYPE_HDMI)) {
			    DE_WRN("screen %d do not support HDMI TYPE!\n", screen_id);
			    continue;
			}

			switch(screen_id) {
			case 0:
				hdmi->channel_id = 0;
				hdmi->name = "hdmi0";
				hdmi->type = DISP_OUTPUT_TYPE_HDMI;
				hdmip->hdmi_clk.clk = MOD_CLK_HDMI;

				hdmip->hdmi_ddc_clk.clk = MOD_CLK_HDMI_DDC;
				hdmip->lcd_clk.clk = MOD_CLK_LCD0CH1;
				hdmip->drc_clk.clk = MOD_CLK_IEPDRC0;
				hdmip->drc_clk.clk_div = 3;
				break;

			case 1:
				hdmi->channel_id = 1;
				hdmi->name = "hdmi1";
				hdmi->type = DISP_OUTPUT_TYPE_HDMI;
				hdmip->hdmi_clk.clk = MOD_CLK_HDMI;
				hdmip->hdmi_ddc_clk.clk = MOD_CLK_HDMI_DDC;
				hdmip->lcd_clk.clk = MOD_CLK_LCD1CH1;
				hdmip->drc_clk.clk = MOD_CLK_IEPDRC1;
				hdmip->drc_clk.clk_div = 3;
				break;

			case 2:
				break;

			default :
				break;
				}
				hdmip->mode = DISP_TV_MOD_720P_50HZ;

				hdmi->init = disp_hdmi_init;
				hdmi->exit = disp_hdmi_exit;

				hdmi->set_func = disp_hdmi_set_func;
				hdmi->enable = disp_hdmi_enable;
				hdmi->disable = disp_hdmi_disable;
				hdmi->is_enabled = disp_hdmi_is_enabled;
				hdmi->set_mode = disp_hdmi_set_mode;
				hdmi->get_mode = disp_hdmi_get_mode;
				hdmi->check_support_mode = disp_hdmi_check_support_mode;
				hdmi->get_input_csc = disp_hdmi_get_input_csc;
				hdmi->hdmi_get_HPD_status = disp_hdmi_get_HPD_status;

				hdmi->init(hdmi);
		}
	}
	return 0;
}
