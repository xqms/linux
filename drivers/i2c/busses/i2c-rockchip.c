/* drivers/i2c/busses/i2c-rk30-adapter.c
 *
 * Copyright (C) 2012 ROCKCHIP, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


//old includes, to check
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <asm/irq.h>




/* Control register */
#define I2C_CON                 0x000
#define I2C_CON_EN              BIT(0)
#define I2C_CON_MOD(mod)        ((mod) << 1)
#define I2C_CON_MOD_TX		0
#define I2C_CON_MOD_TRX		(1 << 1)
#define I2C_CON_MOD_RX		(2 << 1)
#define I2C_CON_MOD_RRX		(3 << 1)
#define I2C_CON_MASK            (3 << 1)
#define I2C_CON_START           BIT(3)
#define I2C_CON_STOP            BIT(4)
#define I2C_CON_LASTACK         BIT(5)
#define I2C_CON_ACTACK          BIT(6)

/* Clock dividor register */
#define I2C_CLKDIV              0x004
#define I2C_CLKDIV_MASK		0xffff
#define I2C_CLKDIV_LSHIFT	0
#define I2C_CLKDIV_HSHIFT	16

/* the slave address accessed  for master rx mode */
#define I2C_MRXADDR             0x008
#define I2C_MRXADDR_LOW         BIT(24)
#define I2C_MRXADDR_MID         BIT(25)
#define I2C_MRXADDR_HIGH        BIT(26)

/* the slave register address accessed for master rx mode */
#define I2C_MRXRADDR            0x00c
#define I2C_MRXRADDR_LOW        BIT(24)
#define I2C_MRXRADDR_MID        BIT(25)
#define I2C_MRXRADDR_HIGH       BIT(26)

/* master tx count */
#define I2C_MTXCNT              0x010

/* master rx count */
#define I2C_MRXCNT              0x014

/* interrupt enable register */
#define I2C_IEN                 0x018
#define I2C_BTFIEN              BIT(0)
#define I2C_BRFIEN              BIT(1)
#define I2C_MBTFIEN             BIT(2)
#define I2C_MBRFIEN             BIT(3)
#define I2C_STARTIEN            BIT(4)
#define I2C_STOPIEN             BIT(5)
#define I2C_NAKRCVIEN           BIT(6)
#define IRQ_MST_ENABLE          (I2C_MBTFIEN | I2C_MBRFIEN | I2C_NAKRCVIEN | I2C_STARTIEN | I2C_STOPIEN)
#define IRQ_ALL_DISABLE         0

/* interrupt pending register */
#define I2C_IPD                 0x01c
#define I2C_BTFIPD              (1 << 0)
#define I2C_BRFIPD              (1 << 1)
#define I2C_MBTFIPD             (1 << 2)
#define I2C_MBRFIPD             (1 << 3)
#define I2C_STARTIPD            (1 << 4)
#define I2C_STOPIPD             (1 << 5)
#define I2C_NAKRCVIPD           (1 << 6)
#define I2C_IPD_ALL_CLEAN       0x7f

/* finished count */
#define I2C_FCNT                0x020

/* I2C tx data register */
#define I2C_TXDATA_BASE         0X100

/* I2C rx data register */
#define I2C_RXDATA_BASE         0x200



struct rockchip_i2c_dev {
	struct platform_device *pdev;
	struct i2c_adapter adap;
	struct clk *clk;
	void __iomem *base;
	int irq;

	u32 bus_clk_rate;
	bool is_suspended;
};

#define ceil(x, y) \
	({ unsigned long __x = (x), __y = (y); (__x + __y - 1) / __y; })

/* SCL Divisor = 8 * (CLKDIVL + CLKDIVH)
 * SCL = i2c_rate/ SCLK Divisor
*/
static void  rockchip_i2c_set_clk(struct rockchip_i2c_dev *i2c_dev,
				  unsigned long bus_clk_rate)
{
	unsigned long i2c_rate = clk_get_rate(i2c_dev->clk);
	unsigned int div, divl, divh;
	u32 val;

	div = ceil(i2c_rate, bus_clk_rate * 8);
	divh = divl = ceil(div, 2);

	val = (((divl & I2C_CLKDIV_MASK) << I2C_CLKDIV_LSHIFT) |
	       ((divh & I2C_CLKDIV_MASK) << I2C_CLKDIV_HSHIFT));
        writel_relaxed(val, i2c_dev->base + I2C_CLKDIV);
        dev_dbg(&i2c_dev->pdev->dev, "set clk(I2C_CLKDIV: 0x%08x)\n", val);
        return;
}


static void rockchip_i2c_init_hw(struct rockchip_i2c_dev *i2c_dev)
{
        rockchip_i2c_set_clk(i2c_dev, i2c_dev->bus_clk_rate);
	return;
}

static irqreturn_t rockchip_i2c_irq(int irq, void *dev_id)
{

	return IRQ_WAKE_THREAD;
}

static irqreturn_t rockchip_i2c_irqthread(int irq, void *dev_id)
{

	return IRQ_HANDLED;
}


static int rockchip_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			     int num)
{
	return -EINVAL;
}

static u32 rockchip_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;
}

static const struct i2c_algorithm rockchip_i2c_algo = {
	.master_xfer		= rockchip_i2c_xfer,
	.functionality		= rockchip_i2c_func,
};



#ifdef CONFIG_OF
static const struct of_device_id rockchip_i2c_of_match[] = {
	{ .compatible = "rockchip,rk2928-i2c", },
	{},
};
MODULE_DEVICE_TABLE(of, rockchip_i2c_of_match);
#endif

static int rockchip_i2c_probe(struct platform_device *pdev)
{
	struct rockchip_i2c_dev *i2c_dev;
	struct resource *res;
	int ret;

	i2c_dev = devm_kzalloc(&pdev->dev, sizeof(struct rockchip_i2c_dev), GFP_KERNEL);
	if (!i2c_dev) {
		dev_err(&pdev->dev, "could not allocate device struct\n");
		return -ENOMEM;
	}

	i2c_dev->pdev = pdev;

	i2c_dev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(i2c_dev->clk)) {
		dev_err(&pdev->dev, "could not retrieve i2c bus clock\n");
		return PTR_ERR(i2c_dev->clk);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENOENT;

	i2c_dev->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2c_dev->base))
		return PTR_ERR(i2c_dev->base);

	i2c_dev->irq = platform_get_irq(pdev, 0);
	ret = devm_request_threaded_irq(&pdev->dev, i2c_dev->irq, rockchip_i2c_irq, rockchip_i2c_irqthread, 0, "rockchip_i2c", i2c_dev);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency",
				   &i2c_dev->bus_clk_rate);
	if (ret)
		i2c_dev->bus_clk_rate = 100000; /* default clock rate */



	i2c_set_adapdata(&i2c_dev->adap, i2c_dev);
	i2c_dev->adap.owner = THIS_MODULE;
	i2c_dev->adap.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	strlcpy(i2c_dev->adap.name, "Rockchip I2C adapter",
		sizeof(i2c_dev->adap.name));
	i2c_dev->adap.retries = 2;
	i2c_dev->adap.algo = &rockchip_i2c_algo;
	i2c_dev->adap.dev.parent = &pdev->dev;
	i2c_dev->adap.nr = pdev->id;
	i2c_dev->adap.dev.of_node = pdev->dev.of_node;

//        i2c->adap.timeout = msecs_to_jiffies(100);

	ret = i2c_add_numbered_adapter(&i2c_dev->adap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add I2C adapter\n");
		return ret;
	}

	platform_set_drvdata(pdev, i2c_dev);
	dev_info(&pdev->dev, "rockchip i2c @ %p, irq %d\n",
		 i2c_dev->base, i2c_dev->irq);

	return 0;
}

static struct platform_driver rockchip_i2c_driver = {
	.probe		= rockchip_i2c_probe,
//	.remove		= rockchip_i2c_remove,
	.driver		= {
		.name	= "rockchip-i2c",
		.owner	= THIS_MODULE,
		.of_match_table = rockchip_i2c_of_match,
//		.pm	= &rockchip_i2c_dev_pm_ops,
	},
};




static int __init rockchip_i2c_init(void)
{
	return platform_driver_register(&rockchip_i2c_driver);
}
subsys_initcall(rockchip_i2c_init);

static void __exit rockchip_i2c_exit(void)
{
	platform_driver_unregister(&rockchip_i2c_driver);
}
module_exit(rockchip_i2c_exit);


////////////////// old

#define I2C_WAIT_TIMEOUT            200

#define rk30_set_bit(p, v, b)        (((p) & ~(1 << (b))) | ((v) << (b)))
#define rk30_get_bit(p, b)           (((p) & (1 << (b))) >> (b))

#define rk30_set_bits(p, v, b, m)	(((p) & ~(m)) | ((v) << (b)))
#define rk30_get_bits(p, b, m)	    (((p) & (m)) >> (b))

#if defined(CONFIG_ARCH_RK30) || defined(CONFIG_ARCH_RK3188)
#define GRF_I2C_CON_BASE            (RK30_GRF_BASE + GRF_SOC_CON1)
#endif
#ifdef CONFIG_ARCH_RK2928
#define GRF_I2C_CON_BASE            (RK2928_GRF_BASE + GRF_SOC_CON1)
#endif

#define I2C_ADAP_SEL_BIT(nr)        ((nr) + 11)
#define I2C_ADAP_SEL_MASK(nr)        ((nr) + 27)
enum rk30_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP
};

struct rk30_i2c {
	spinlock_t		lock;
	wait_queue_head_t	wait;
	unsigned int		suspended:1;

	struct i2c_msg		*msg;
        union {
	        unsigned int		msg_num;
	        unsigned int		is_busy;
        };
        union {
	        unsigned int		msg_idx;
	        int		        error;
        };
	unsigned int		msg_ptr;

	unsigned int		irq;

	enum rk30_i2c_state	state;
        unsigned int            complete_what;
	unsigned long		clkrate;

	void __iomem		*regs;
        void __iomem        *con_base;
	struct clk		    *clk;
	struct device		*dev;
	struct resource		*ioarea;
	struct i2c_adapter	adap;
    
        unsigned long		scl_rate;
	unsigned long		i2c_rate;
        unsigned int        addr;
        unsigned int        mode;
        unsigned int        count;

	int sda_mode, scl_mode;


        void (*i2c_init_hw)(struct rk30_i2c *, unsigned long scl_rate);
        void (*i2c_set_clk)(struct rk30_i2c *, unsigned long);
        irqreturn_t (*i2c_irq)(int, void *);
};

#define COMPLETE_READ     (1<<STATE_START|1<<STATE_READ|1<<STATE_STOP)
#define COMPLETE_WRITE     (1<<STATE_START|1<<STATE_WRITE|1<<STATE_STOP)



static inline void rk30_i2c_enable(struct rk30_i2c *i2c, unsigned int lastnak)
{
        unsigned int con = 0;

        con |= I2C_CON_EN;
        con |= I2C_CON_MOD(i2c->mode);
        if(lastnak)
                con |= I2C_CON_LASTACK;
        con |= I2C_CON_START;
        writel_relaxed(con, i2c->regs + I2C_CON);
}

static inline void rk30_i2c_clean_start(struct rk30_i2c *i2c)
{
        unsigned int con = readl_relaxed(i2c->regs + I2C_CON);

        con &= ~I2C_CON_START;
        writel_relaxed(con, i2c->regs + I2C_CON);
}
static inline void rk30_i2c_send_stop(struct rk30_i2c *i2c)
{
        unsigned int con = readl_relaxed(i2c->regs + I2C_CON);

        con |= I2C_CON_STOP;
        if(con & I2C_CON_START)
                dev_warn(i2c->dev, "I2C_CON: start bit is set\n");
        
        writel_relaxed(con, i2c->regs + I2C_CON);
}
static inline void rk30_i2c_clean_stop(struct rk30_i2c *i2c)
{
        unsigned int con = readl_relaxed(i2c->regs + I2C_CON);

        con &= ~I2C_CON_STOP;
        writel_relaxed(con, i2c->regs + I2C_CON);
}

/* returns TRUE if we reached the end of the current message */
static inline int is_msgend(struct rk30_i2c *i2c)
{
	return i2c->msg_ptr >= i2c->msg->len;
}

static void rk30_i2c_stop(struct rk30_i2c *i2c, int ret)
{

        i2c->msg_ptr = 0;
	i2c->msg = NULL;
        if(ret == -EAGAIN){
                i2c->state = STATE_IDLE;
                i2c->is_busy = 0;
                wake_up(&i2c->wait);
                return;
        }
	i2c->error = ret;
        writel_relaxed(I2C_STOPIEN, i2c->regs + I2C_IEN);
        i2c->state = STATE_STOP;
        rk30_i2c_send_stop(i2c);
        return;
}
static inline void rk30_set_rx_mode(struct rk30_i2c *i2c, unsigned int lastnak)
{
        unsigned long con = readl_relaxed(i2c->regs + I2C_CON);

        con &= (~I2C_CON_MASK);
        con |= (I2C_CON_MOD_RX << 1);
        if(lastnak)
                con |= I2C_CON_LASTACK;
        writel_relaxed(con, i2c->regs + I2C_CON);
}

static void rk30_irq_read_prepare(struct rk30_i2c *i2c)
{
    unsigned int cnt, len = i2c->msg->len - i2c->msg_ptr;

    if(len <= 32 && i2c->msg_ptr != 0) 
            rk30_set_rx_mode(i2c, 1);
    else if(i2c->msg_ptr != 0)
            rk30_set_rx_mode(i2c, 0);

    if(is_msgend(i2c)) {
        rk30_i2c_stop(i2c, i2c->error);
        return;
    }
    if(len > 32)
        cnt = 32;
    else
        cnt = len;
    writel_relaxed(cnt, i2c->regs + I2C_MRXCNT);
}
static void rk30_irq_read_get_data(struct rk30_i2c *i2c)
{
     unsigned int i, len = i2c->msg->len - i2c->msg_ptr;
     unsigned int p = 0;

     len = (len >= 32)?32:len;

     for(i = 0; i < len; i++){
         if(i%4 == 0)
             p = readl_relaxed(i2c->regs + I2C_RXDATA_BASE +  (i/4) * 4);
         i2c->msg->buf[i2c->msg_ptr++] = (p >>((i%4) * 8)) & 0xff;
    }

     return;
}

static void rk30_irq_write_prepare(struct rk30_i2c *i2c)
{
    unsigned int data = 0, cnt = 0, i, j;
    unsigned char byte;

    if(is_msgend(i2c)) {
        rk30_i2c_stop(i2c, i2c->error);
        return;
    }
    for(i = 0; i < 8; i++){
        data = 0;
        for(j = 0; j < 4; j++) {
            if(is_msgend(i2c)) 
                break;
            if(i2c->msg_ptr == 0 && cnt == 0)
                byte = (i2c->addr & 0x7f) << 1;
            else
                byte =  i2c->msg->buf[i2c->msg_ptr++];
            cnt++;
            data |= (byte << (j * 8));
        }
        writel_relaxed(data, i2c->regs + I2C_TXDATA_BASE + 4 * i);
        if(is_msgend(i2c)) 
            break;
    }
    writel_relaxed(cnt, i2c->regs + I2C_MTXCNT);
}

static void rk30_i2c_irq_nextblock(struct rk30_i2c *i2c, unsigned int ipd)
{
        switch (i2c->state) {
        case STATE_START:
                if(!(ipd & I2C_STARTIPD)){
                        rk30_i2c_stop(i2c, -ENXIO);
                        dev_err(i2c->dev, "Addr[0x%02x] no start irq in STATE_START\n", i2c->addr);
                        writel_relaxed(I2C_IPD_ALL_CLEAN, i2c->regs + I2C_IPD);
                        goto out;
                }
                i2c->complete_what |= 1<<i2c->state;
                writel_relaxed(I2C_STARTIPD, i2c->regs + I2C_IPD);
                rk30_i2c_clean_start(i2c);
                if(i2c->mode ==  I2C_CON_MOD_TX){
                        writel_relaxed(I2C_MBTFIEN  | I2C_NAKRCVIEN, i2c->regs + I2C_IEN);
                        i2c->state = STATE_WRITE;
                        goto prepare_write;
                } else {
                        writel_relaxed(I2C_MBRFIEN | I2C_NAKRCVIEN, i2c->regs + I2C_IEN);
                        i2c->state = STATE_READ;
                        goto prepare_read;
                }
        case STATE_WRITE:
                if(!(ipd & I2C_MBTFIPD)){
                        rk30_i2c_stop(i2c, -ENXIO);
                        dev_err(i2c->dev, "Addr[0x%02x] no mbtf irq in STATE_WRITE\n", i2c->addr);
                        writel_relaxed(I2C_IPD_ALL_CLEAN, i2c->regs + I2C_IPD);
                        goto out;
                }
                i2c->complete_what |= 1<<i2c->state;
                writel_relaxed(I2C_MBTFIPD, i2c->regs + I2C_IPD);
prepare_write:
                rk30_irq_write_prepare(i2c);
                break;
        case STATE_READ:
                if(!(ipd & I2C_MBRFIPD)){
                        rk30_i2c_stop(i2c, -ENXIO);
                        dev_err(i2c->dev, "Addr[0x%02x] no mbrf irq in STATE_READ, ipd = 0x%x\n", i2c->addr, ipd);
                        writel_relaxed(I2C_IPD_ALL_CLEAN, i2c->regs + I2C_IPD);
                        goto out;
                }
                i2c->complete_what |= 1<<i2c->state;
                writel_relaxed(I2C_MBRFIPD, i2c->regs + I2C_IPD);
                rk30_irq_read_get_data(i2c);
prepare_read:
                rk30_irq_read_prepare(i2c);
                break;
        case STATE_STOP:
                if(!(ipd & I2C_STOPIPD)){
                        rk30_i2c_stop(i2c, -ENXIO);
                        dev_err(i2c->dev, "Addr[0x%02x] no stop irq in STATE_STOP\n", i2c->addr);
                        writel_relaxed(I2C_IPD_ALL_CLEAN, i2c->regs + I2C_IPD);
                        goto out;
                }
                rk30_i2c_clean_stop(i2c);
                writel_relaxed(I2C_STOPIPD, i2c->regs + I2C_IPD);
	        i2c->is_busy = 0;
                i2c->complete_what |= 1<<i2c->state;
                i2c->state = STATE_IDLE;
	        wake_up(&i2c->wait);
                break;
        default:
                break;
        }
out:
        return;
}
static irqreturn_t rk30_i2c_irq(int irq, void *dev_id)
{
        struct rk30_i2c *i2c = dev_id;
        unsigned int ipd;

        spin_lock(&i2c->lock);
        ipd = readl_relaxed(i2c->regs + I2C_IPD);
        if(i2c->state == STATE_IDLE){
                dev_info(i2c->dev, "Addr[0x%02x]  irq in STATE_IDLE, ipd = 0x%x\n", i2c->addr, ipd);
                writel_relaxed(I2C_IPD_ALL_CLEAN, i2c->regs + I2C_IPD);
                goto out;
        }

        if(ipd & I2C_NAKRCVIPD){
                writel_relaxed(I2C_NAKRCVIPD, i2c->regs + I2C_IPD);
                i2c->error = -EAGAIN;
                goto out;
        }
        rk30_i2c_irq_nextblock(i2c, ipd);
out:
        spin_unlock(&i2c->lock);
        return IRQ_HANDLED;
}


static int rk30_i2c_set_master(struct rk30_i2c *i2c, struct i2c_msg *msgs, int num)
{
        unsigned int addr = (msgs[0].addr & 0x7f) << 1;
        unsigned int reg_valid_bits = 0;
        unsigned int reg_addr = 0;
    
        if(num == 1) {
                i2c->count = msgs[0].len;
                if(!(msgs[0].flags & I2C_M_RD)){
                        i2c->msg = &msgs[0];
                        i2c->mode = I2C_CON_MOD_TX;
                }
                else {
                        addr |= 1;
                        i2c->msg = &msgs[0];
                        writel_relaxed(addr | I2C_MRXADDR_LOW, i2c->regs + I2C_MRXADDR);
                        writel_relaxed(0, i2c->regs + I2C_MRXRADDR);
                        i2c->mode = I2C_CON_MOD_TRX;
                        //i2c->mode = I2C_CON_MOD_RX;
                }
        }
        else if(num == 2) {
                i2c->count = msgs[1].len;
                switch(msgs[0].len){
                case 1:
                        reg_addr = msgs[0].buf[0];
                        reg_valid_bits |= I2C_MRXADDR_LOW;
                        break;
                case 2:
                        reg_addr = msgs[0].buf[0] | (msgs[0].buf[1] << 8);
                        reg_valid_bits |= I2C_MRXADDR_LOW | I2C_MRXADDR_MID;
                        break;
                case 3:
                        reg_addr = msgs[0].buf[0] | (msgs[0].buf[1] << 8) | (msgs[0].buf[2] << 16);
                        reg_valid_bits |= I2C_MRXADDR_LOW | I2C_MRXADDR_MID | I2C_MRXADDR_HIGH;
                        break;
                default:
                        return -EIO;
                }
                if((msgs[0].flags & I2C_M_RD) && (msgs[1].flags & I2C_M_RD)) {
                        addr |= 1;
                        i2c->msg = &msgs[1];
                        writel_relaxed(addr | I2C_MRXADDR_LOW, i2c->regs + I2C_MRXADDR);
                        writel_relaxed(reg_addr | reg_valid_bits, i2c->regs + I2C_MRXRADDR);
                        i2c->mode = I2C_CON_MOD_RRX;
                }
                else if(!(msgs[0].flags & I2C_M_RD) && (msgs[1].flags & I2C_M_RD)) {
                        i2c->msg = &msgs[1];
                        writel_relaxed(addr | I2C_MRXADDR_LOW, i2c->regs + I2C_MRXADDR);
                        writel_relaxed(reg_addr | reg_valid_bits, i2c->regs + I2C_MRXRADDR);
                        i2c->mode = I2C_CON_MOD_TRX;
                }
                else 
                        return -EIO;
        }
        else {
                dev_err(i2c->dev, "This case(num > 2) has not been support now\n");
                return -EIO;
        }

        return 0;
}

/* rk30_i2c_xfer
 *
 * first port of call from the i2c bus code when an message needs
 * transferring across the i2c bus.
*/

static int rk30_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	int ret = 0;
        unsigned long scl_rate;
	struct rk30_i2c *i2c = (struct rk30_i2c *)adap->algo_data;
	unsigned long timeout, flags;
        int error = 0;
        /* 32 -- max transfer bytes
         * 2 -- addr bytes * 2
         * 3 -- max reg addr bytes
         * 9 -- cycles per bytes
         * max cycles: (32 + 2 + 3) * 9 --> 400 cycles
         */
        int msleep_time = 400 * 1000/ i2c->scl_rate; // ms

        clk_enable(i2c->clk);

//	rockchip_i2c_set_clkdiv(i2c, scl_rate);
        dev_dbg(i2c->dev, "i2c transfer start: addr: 0x%x, scl_reate: %ldKhz, len: %d\n", msgs[0].addr, scl_rate/1000, num);


	spin_lock_irqsave(&i2c->lock, flags);
	if(rk30_i2c_set_master(i2c, msgs, num) < 0){
	        spin_unlock_irqrestore(&i2c->lock, flags);
                dev_err(i2c->dev, "addr[0x%02x] set master error\n", msgs[0].addr);  
    	        ret = -EIO;
		goto out;
        }
        i2c->addr = msgs[0].addr;
        i2c->msg_ptr = 0;
        i2c->error = 0;
	i2c->is_busy = 1;
        i2c->state = STATE_START;
        i2c->complete_what = 0;
        writel_relaxed(I2C_STARTIEN, i2c->regs + I2C_IEN);
	spin_unlock_irqrestore(&i2c->lock, flags);

        rk30_i2c_enable(i2c, (i2c->count > 32)?0:1); //if count > 32,  byte(32) send ack

        if (in_atomic()){
                int tmo = I2C_WAIT_TIMEOUT * USEC_PER_MSEC;
                while(tmo-- && i2c->is_busy != 0)
                        udelay(1);
                timeout = (tmo <= 0)?0:1;
        }else
	        timeout = wait_event_timeout(i2c->wait, (i2c->is_busy == 0), msecs_to_jiffies(I2C_WAIT_TIMEOUT));

	spin_lock_irqsave(&i2c->lock, flags);
        i2c->state = STATE_IDLE;
        error = i2c->error;
	spin_unlock_irqrestore(&i2c->lock, flags);

	if (timeout == 0){
                if(error < 0)
                        dev_dbg(i2c->dev, "error = %d\n", error);
                else if((i2c->complete_what !=COMPLETE_READ  && i2c->complete_what != COMPLETE_WRITE)){
                        dev_err(i2c->dev, "Addr[0x%02x] wait event timeout, state: %d, is_busy: %d, error: %d, complete_what: 0x%x, ipd: 0x%x\n", 
                                msgs[0].addr, i2c->state, i2c->is_busy, error, i2c->complete_what, readl_relaxed(i2c->regs + I2C_IPD));  
                        error = -ETIMEDOUT;
                        msleep(msleep_time);
                        rk30_i2c_send_stop(i2c);
                        msleep(1);
                }
                else
                        dev_dbg(i2c->dev, "Addr[0x%02x] wait event timeout, but transfer complete\n", i2c->addr);  
        }
        writel_relaxed(I2C_IPD_ALL_CLEAN, i2c->regs + I2C_IPD);
        writel_relaxed(IRQ_ALL_DISABLE, i2c->regs + I2C_IEN);
        writel_relaxed( 0, i2c->regs + I2C_CON);

        if(error == -EAGAIN)
                dev_dbg(i2c->dev, "No ack(complete_what: 0x%x), Maybe slave(addr: 0x%02x) not exist or abnormal power-on\n",
                                i2c->complete_what, i2c->addr);


out:
        dev_dbg(i2c->dev, "i2c transfer stop: addr: 0x%x, state: %d, ret: %d\n", msgs[0].addr, ret, i2c->state);

        clk_disable(i2c->clk);
	return (ret < 0)?ret:num;
}

/* i2c bus registration info */

static int i2c_max_adap = 0;
void i2c_adap_sel(struct rk30_i2c *i2c, int nr, int adap_type)
{
        writel_relaxed((1 << I2C_ADAP_SEL_BIT(nr)) | (1 << I2C_ADAP_SEL_MASK(nr)) ,
                        i2c->con_base);
}

/* rk30_i2c_probe
 *
 * called by the bus driver when a suitable device is found
*/

static int rk30_i2c_probe(struct platform_device *pdev)
{
	struct rk30_i2c *i2c = NULL;
	struct rk30_i2c_platform_data *pdata = NULL;
	struct resource *res;
	int ret;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}


	i2c = kzalloc(sizeof(struct rk30_i2c), GFP_KERNEL);
	if (!i2c) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}
//        i2c->con_base = (void __iomem *)GRF_I2C_CON_BASE;
//        i2c_adap_sel(i2c, pdata->bus_num, pdata->adap_type);


	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

	/* find the clock and enable it */

	i2c->dev = &pdev->dev;
	i2c->clk = clk_get(&pdev->dev, "i2c");
	if (IS_ERR(i2c->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		ret = -ENOENT;
		goto err_noclk;
	}

	dev_dbg(&pdev->dev, "clock source %p\n", i2c->clk);

	clk_enable(i2c->clk);

	/* map the registers */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		ret = -ENOENT;
		goto err_get_resource;
	}

	i2c->ioarea = request_mem_region(res->start, resource_size(res),
					 pdev->name);

	if (i2c->ioarea == NULL) {
		dev_err(&pdev->dev, "cannot request IO\n");
		ret = -ENXIO;
		goto err_ioarea;
	}

	i2c->regs = ioremap(res->start, resource_size(res));

	if (i2c->regs == NULL) {
		dev_err(&pdev->dev, "cannot map IO\n");
		ret = -ENXIO;
		goto err_ioremap;
	}

	dev_dbg(&pdev->dev, "registers %p (%p, %p)\n",
		i2c->regs, i2c->ioarea, res);

	/* setup info block for the i2c core */

//        i2c->i2c_init_hw = &rockchip_i2c_init_hw;

	/* find the IRQ for this unit (note, this relies on the init call to
	 * ensure no current IRQs pending
	 */

	i2c->irq = ret = platform_get_irq(pdev, 0);
	if (ret <= 0) {
		dev_err(&pdev->dev, "cannot find IRQ\n");
		goto err_get_irq;
	}

	ret = request_irq(i2c->irq, i2c->i2c_irq, IRQF_DISABLED,
			  dev_name(&pdev->dev), i2c);

	if (ret != 0) {
		dev_err(&pdev->dev, "cannot claim IRQ %d\n", i2c->irq);
		goto err_request_irq;
	}

	platform_set_drvdata(pdev, i2c);

        i2c->i2c_init_hw(i2c, 100 * 1000);
        i2c_max_adap++;
	return 0;

err_request_irq:
err_get_irq:
	i2c_del_adapter(&i2c->adap);
	iounmap(i2c->regs);
err_ioremap:
	kfree(i2c->ioarea);
err_ioarea:
	release_resource(i2c->ioarea);
err_get_resource:
	clk_put(i2c->clk);
err_noclk:
	kfree(i2c);
	return ret;
}

/* rk30_i2c_remove
 *
 * called when device is removed from the bus
*/

static int rk30_i2c_remove(struct platform_device *pdev)
{
	struct rk30_i2c *i2c = platform_get_drvdata(pdev);

	free_irq(i2c->irq, i2c);
	i2c_del_adapter(&i2c->adap);
	iounmap(i2c->regs);
	kfree(i2c->ioarea);
	release_resource(i2c->ioarea);
	clk_put(i2c->clk);
	kfree(i2c);
	return 0;
}



MODULE_DESCRIPTION("Driver for RK30 I2C Bus");
MODULE_AUTHOR("kfx, kfx@rock-chips.com");
MODULE_LICENSE("GPL");
