

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/pinctrl/consumer.h>
#include <securec.h>

#include "securec.h"
#include "hwsensor.h"
#include "sensor_commom.h"
#include "hw_csi.h"

#define I2S(i) container_of(i, sensor_t, intf)
#define Sensor2Pdev(s) container_of((s).dev, struct platform_device, dev)
static bool power_on_status = false;//false: power off, true:power on
/*lint -e826 -e31 -e485 -e785 -e731 -e846 -e514 -e866 -e30 -e84 -e838 -e64 -e528 -e753 -e749 -e715 -esym(826, 31, 485, 785, 731, 846, 514, 866, 30, 84, 838, 64, 528, 753, 749, 715*)*/
/*lint -save -e826 -e31 -e485 -e785 -e731 -e846 -e514 -e866 -e30 -e84 -e838 -e64 -e528 -e753 -e749 -e715 -specific(-e826 -e31 -e485 -e785 -e731 -e846 -e514 -e866 -e30 -e84 -e838 -e64 -e528 -e753 -e749 -e715)*/
//lint -save -e747
char const* sp2509_front_get_name(hwsensor_intf_t* si);
int sp2509_front_config(hwsensor_intf_t* si, void  *argp);
int sp2509_front_power_up(hwsensor_intf_t* si);
int sp2509_front_power_down(hwsensor_intf_t* si);
int sp2509_front_match_id(hwsensor_intf_t* si, void * data);
int sp2509_front_csi_enable(hwsensor_intf_t* si);
int sp2509_front_csi_disable(hwsensor_intf_t* si);


static hwsensor_vtbl_t s_sp2509_front_vtbl =
{
    .get_name = sp2509_front_get_name,
    .config = sp2509_front_config,
    .power_up = sp2509_front_power_up,
    .power_down = sp2509_front_power_down,
    .match_id = sp2509_front_match_id,
    .csi_enable = sp2509_front_csi_enable,
    .csi_disable = sp2509_front_csi_disable,
};

static struct sensor_power_setting sp2509_front_power_up_setting[] = {
//disable main camera reset
    {
        .seq_type = SENSOR_SUSPEND,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },
    //MCAM1 IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .data = (void*)"main-sensor-iovdd",
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    //SCAM DVDD1.2V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_1P2V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
	
    //SCAM AVDD 2.80V
    {
        .seq_type = SENSOR_AVDD,
        .data = (void*)"slave-sensor-avdd",
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 5,
    },
    //MCAM1 DVDD 1.05V
    {
        .seq_type = SENSOR_DVDD2,
        .config_val = LDO_VOLTAGE_1P2V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },

    



    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_PWDN,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 5,
    },
    //MCAM XSHUTDOWN RST
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_HIGH,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 5,
    },
};
static struct sensor_power_setting sp2509_front_power_down_setting[] = {

    //MCAM XSHUTDOWN RST
    {
        .seq_type = SENSOR_RST,
        .config_val = SENSOR_GPIO_LOW,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },

    {
        .seq_type = SENSOR_MCLK,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    {
        .seq_type = SENSOR_PWDN,
        .config_val = SENSOR_GPIO_HIGH,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },
    //MCAM1 DVDD 1.05V
    {
        .seq_type = SENSOR_DVDD2,
        .config_val = LDO_VOLTAGE_1P2V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },

    //SCAM DVDD1.2V
    {
        .seq_type = SENSOR_DVDD,
        .config_val = LDO_VOLTAGE_1P2V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },

    //SCAM AVDD 2.80V
    {
        .seq_type = SENSOR_AVDD,
        .data = (void*)"slave-sensor-avdd",
        .config_val = LDO_VOLTAGE_V2P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 0,
    },

    //MCAM1 IOVDD 1.80V
    {
        .seq_type = SENSOR_IOVDD,
        .data = (void*)"main-sensor-iovdd",
        .config_val = LDO_VOLTAGE_1P8V,
        .sensor_index = SENSOR_INDEX_INVALID,
        .delay = 1,
    },

};

struct mutex sp2509_front_power_lock;
atomic_t volatile sp2509_front_powered = ATOMIC_INIT(0);
static sensor_t s_sp2509_front =
{
    .intf = { .vtbl = &s_sp2509_front_vtbl, },
    .power_setting_array = {
        .size = ARRAY_SIZE(sp2509_front_power_up_setting),
        .power_setting = sp2509_front_power_up_setting,
    },
    .power_down_setting_array = {
        .size = ARRAY_SIZE(sp2509_front_power_down_setting),
        .power_setting = sp2509_front_power_down_setting,
    },
    .p_atpowercnt = &sp2509_front_powered,
};

static const struct of_device_id s_sp2509_front_dt_match[] =
{
    {
        .compatible = "huawei,sp2509AGASSI2",
        .data = &s_sp2509_front.intf,
    },
    {
    },
};

MODULE_DEVICE_TABLE(of, s_sp2509_front_dt_match);

static struct platform_driver s_sp2509_front_driver =
{
    .driver =
    {
        .name = "huawei,sp2509AGASSI2",
        .owner = THIS_MODULE,
        .of_match_table = s_sp2509_front_dt_match,
    },
};

char const* sp2509_front_get_name(hwsensor_intf_t* si)
{
    sensor_t* sensor = I2S(si);
    return sensor->board_info->name;
}

int sp2509_front_power_up(hwsensor_intf_t* si)
{
    int ret = 0;
    sensor_t* sensor = NULL;
    sensor = I2S(si);
    cam_info("enter %s. index = %d name = %s", __func__, sensor->board_info->sensor_index, sensor->board_info->name);

    ret = hw_sensor_power_up_config(s_sp2509_front.dev, sensor->board_info);
    if (0 == ret ){
        cam_info("%s. power up config success.", __func__);
    }else{
        cam_err("%s. power up config fail.", __func__);
        return ret;
    }
    if (hw_is_fpga_board()) {
        ret = do_sensor_power_on(sensor->board_info->sensor_index, sensor->board_info->name);
    } else {
        ret = hw_sensor_power_up(sensor);
    }
    if (0 == ret )
    {
        cam_info("%s. power up sensor success.", __func__);
    }
    else
    {
        cam_err("%s. power up sensor fail.", __func__);
    }
    return ret;
}

int sp2509_front_power_down(hwsensor_intf_t* si)
{
    int ret = 0;
    sensor_t* sensor = NULL;
    sensor = I2S(si);
    cam_info("111enter %s. index = %d name = %s", __func__, sensor->board_info->sensor_index, sensor->board_info->name);
    if (hw_is_fpga_board()) {
        ret = do_sensor_power_off(sensor->board_info->sensor_index, sensor->board_info->name);
    } else {
        ret = hw_sensor_power_down(sensor);
    }
    if (0 == ret )
    {
        cam_info("%s. power down sensor success.", __func__);
    }
    else
    {
        cam_err("%s. power down sensor fail.", __func__);
    }
    hw_sensor_power_down_config(sensor->board_info);
    return ret;
}

int sp2509_front_csi_enable(hwsensor_intf_t* si)
{
    return 0;
}

int sp2509_front_csi_disable(hwsensor_intf_t* si)
{
    return 0;
}

int sp2509_front_match_id(hwsensor_intf_t* si, void * data)
{

   sensor_t* sensor = NULL;
    struct sensor_cfg_data *cdata = NULL;
    int error = 0;
    cam_info("%s enter.", __func__);
    if(NULL == si || NULL == data)
    {
        cam_err("%s. si or data is NULL.", __func__);
        return -EINVAL;
    }

    sensor = I2S(si);
    if (NULL == sensor || NULL == sensor->board_info || NULL == sensor->board_info->name) {
        cam_err("%s. sensor or board_info is NULL.", __func__);
        return -EINVAL;
    }
    cdata = (struct sensor_cfg_data *)data;

    error = memset_s(cdata->cfg.name, DEVICE_NAME_SIZE, 0, DEVICE_NAME_SIZE);
    if (error != EOK) {
        cam_err("%s. failed to memset_s write_data.", __func__);
    }
    strncpy_s(cdata->cfg.name, sizeof(cdata->cfg.name), sensor->board_info->name, DEVICE_NAME_SIZE - 1);
    cdata->data = sensor->board_info->sensor_index;

    return 0;
}

int sp2509_front_config(hwsensor_intf_t* si, void  *argp)
{
    struct sensor_cfg_data *data;

    int ret =0;

    if (NULL == si || NULL == argp){
        cam_err("%s : si or argp is null", __func__);
        return -1;
    }

    data = (struct sensor_cfg_data *)argp;
    cam_debug("sp2509_front cfgtype = %d",data->cfgtype);
    switch(data->cfgtype){
        case SEN_CONFIG_POWER_ON:
            mutex_lock(&sp2509_front_power_lock);
            if(false == power_on_status){
                ret = si->vtbl->power_up(si);
                if (ret == 0) {
                    power_on_status = true;
                }
            }
            /*lint -e455 -esym(455,*)*/
            mutex_unlock(&sp2509_front_power_lock);
            /*lint -e455 +esym(455,*)*/
            break;
        case SEN_CONFIG_POWER_OFF:
            mutex_lock(&sp2509_front_power_lock);
            if(true == power_on_status){
                ret = si->vtbl->power_down(si);
                if (ret != 0) {
                    cam_err("%s. power_down fail.", __func__);
                }
                power_on_status = false;
            }
            /*lint -e455 -esym(455,*)*/
            mutex_unlock(&sp2509_front_power_lock);
            /*lint -e455 +esym(455,*)*/
            break;
        case SEN_CONFIG_WRITE_REG:
            break;
        case SEN_CONFIG_READ_REG:
            break;
        case SEN_CONFIG_WRITE_REG_SETTINGS:
            break;
        case SEN_CONFIG_READ_REG_SETTINGS:
            break;
        case SEN_CONFIG_ENABLE_CSI:
            break;
        case SEN_CONFIG_DISABLE_CSI:
            break;
        case SEN_CONFIG_MATCH_ID:
            ret = si->vtbl->match_id(si,argp);
            break;
        default:
            cam_err("%s cfgtype(%d) is error", __func__, data->cfgtype);
            break;
    }

    return ret;
}

static int32_t sp2509_front_platform_probe(struct platform_device* pdev)
{
    int rc = 0;
    cam_notice("enter %s",__func__);

    if (pdev->dev.of_node) {
        rc = hw_sensor_get_dt_data(pdev, &s_sp2509_front);
        if (rc < 0) {
            cam_err("%s failed line %d\n", __func__, __LINE__);
            goto sp2509_front_sensor_probe_fail;
        }
    } else {
        cam_err("%s sp2509_front of_node is NULL.\n", __func__);
        goto sp2509_front_sensor_probe_fail;
    }
    s_sp2509_front.dev = &pdev->dev;
    mutex_init(&sp2509_front_power_lock);
    rc = hwsensor_register(pdev, &s_sp2509_front.intf);
    rc = rpmsg_sensor_register(pdev, (void*)&s_sp2509_front);

sp2509_front_sensor_probe_fail:
    return rc;
}

static int __init sp2509_front_init_module(void)
{
    cam_info("Enter: %s", __func__);
    return platform_driver_probe(&s_sp2509_front_driver,
            sp2509_front_platform_probe);
}

static void __exit sp2509_front_exit_module(void)
{
    hwsensor_unregister(Sensor2Pdev(s_sp2509_front));
    platform_driver_unregister(&s_sp2509_front_driver);
}
/*lint -restore*/
/*lint -e826 -e31 -e485 -e785 -e731 -e846 -e514 -e866 -e30 -e84 -e838 -e64 -e528 -e753 -e749 +esym(826, 31, 485, 785, 731, 846, 514, 866, 30, 84, 838, 64, 528, 753, 749, 715*)*/
/*lint -e528 -esym(528,*)*/
module_init(sp2509_front_init_module);
module_exit(sp2509_front_exit_module);
/*lint -e528 +esym(528,*)*/
/*lint -e753 -esym(753,*)*/
MODULE_DESCRIPTION("sp2509_front");
MODULE_LICENSE("GPL v2");
/*lint -e753 +esym(753,*)*/
