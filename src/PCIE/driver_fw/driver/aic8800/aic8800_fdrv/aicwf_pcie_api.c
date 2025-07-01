#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <asm/delay.h>
#include "aicwf_pcie_api.h"


void aicwf_pcie_cfg_x(struct aic_pci_dev *adev, u32 addr, u32 wr)
{
	//u32  rd, bk;
	//pci_read_config_dword (adev->pdev, addr, &rd);
	pci_write_config_dword(adev->pdev, addr,  wr);
	//pci_read_config_dword (adev->pdev, addr, &bk);
    //LOG_INFO("w %08x %08x, %08x -> %08x", addr, wr, rd, bk);
}

void aic_pcie_map_set(struct aic_pci_dev *adev, u8 idx, u32 base, u32 limt, u32 addr)
{
    u32 bar0 ;
    // set aic pcie map table, tbl[idx] map [base] to [addr] in [limt] on bar0
    LOG_INFO("map %d: %04x~%04x -> %08x", idx, base, limt, addr);
    bar0 = adev->bar0;
    aicwf_pcie_cfg_x(adev, 0x900, 0x80000000 + idx);
    aicwf_pcie_cfg_x(adev, 0x90C, bar0 + base);
    aicwf_pcie_cfg_x(adev, 0x910, 0);
    aicwf_pcie_cfg_x(adev, 0x914, bar0 + limt);
    aicwf_pcie_cfg_x(adev, 0x918, addr);
    aicwf_pcie_cfg_x(adev, 0x91C, 0);
    aicwf_pcie_cfg_x(adev, 0x904, 0);
    aicwf_pcie_cfg_x(adev, 0x908, 0x80000000);
}

void aicwf_pcie_cfg(struct aic_pci_dev *adev)
{
    //u8* addr;
    //u32 rd, wr;
    //1. aic pcie config
    //# set 0 <linkctl clkreq en>, <aspm l1.1>, <aspm l1.2>
    if (adev->chip_id == PRODUCT_ID_AIC8800D80X2) {
        pcie_capability_clear_word(adev->pdev, PCI_EXP_LNKCTL, PCI_EXP_LNKCTL_CLKREQ_EN);
        pcie_capability_clear_word(adev->pdev, PCI_L1SS_CTL1 , PCI_L1SS_CTL1_ASPM_L1_1 );
        pcie_capability_clear_word(adev->pdev, PCI_L1SS_CTL1 , PCI_L1SS_CTL1_ASPM_L1_2 );
    }

    //2. mmap aic chip domains
    if (adev->chip_id == PRODUCT_ID_AIC8800D80) {
        LOG_INFO("host dma");
        aic_pcie_map_set(adev, 0, AIC_MAP_HDMA_BASE_AIC8800D80, AIC_MAP_HDMA_LIMT_AIC8800D80, AIC_MAP_HDMA_ADDR_AIC8800D80);
        LOG_INFO("top pcie");
        aic_pcie_map_set(adev, 1, AIC_MAP_TPCI_BASE_AIC8800D80, AIC_MAP_TPCI_LIMT_AIC8800D80, AIC_MAP_TPCI_ADDR_AIC8800D80);
        LOG_INFO("mail box");
        aic_pcie_map_set(adev, 2, AIC_MAP_MBOX_BASE_AIC8800D80, AIC_MAP_MBOX_LIMT_AIC8800D80, AIC_MAP_MBOX_ADDR_AIC8800D80);
        LOG_INFO("cpu sys");
        aic_pcie_map_set(adev, 3, AIC_MAP_SCTL_BASE_AIC8800D80, AIC_MAP_SCTL_LIMT_AIC8800D80, AIC_MAP_SCTL_ADDR_AIC8800D80);
        LOG_INFO("share mem");
        aic_pcie_map_set(adev, 4, AIC_MAP_SHRM_BASE_AIC8800D80, AIC_MAP_SHRM_LIMT_AIC8800D80, AIC_MAP_SHRM_ADDR_AIC8800D80);

        adev->emb_hdma = adev->map0 + AIC_MAP_HDMA_BASE_AIC8800D80;
        adev->emb_tpci = adev->map0 + AIC_MAP_TPCI_BASE_AIC8800D80;
        adev->emb_mbox = adev->map0 + AIC_MAP_MBOX_BASE_AIC8800D80;
        adev->emb_sctl = adev->map0 + AIC_MAP_SCTL_BASE_AIC8800D80;
        adev->emb_shrm = adev->map0 + AIC_MAP_SHRM_BASE_AIC8800D80;
    } else if (adev->chip_id == PRODUCT_ID_AIC8800D80X2) {
        LOG_INFO("host dma");
        aic_pcie_map_set(adev, 0, AIC_MAP_HDMA_BASE_AIC8800D80X2, AIC_MAP_HDMA_LIMT_AIC8800D80X2, AIC_MAP_HDMA_ADDR_AIC8800D80X2);
        LOG_INFO("top pcie");
        aic_pcie_map_set(adev, 1, AIC_MAP_TPCI_BASE_AIC8800D80X2, AIC_MAP_TPCI_LIMT_AIC8800D80X2, AIC_MAP_TPCI_ADDR_AIC8800D80X2);
        LOG_INFO("mail box");
        aic_pcie_map_set(adev, 2, AIC_MAP_MBOX_BASE_AIC8800D80X2, AIC_MAP_MBOX_LIMT_AIC8800D80X2, AIC_MAP_MBOX_ADDR_AIC8800D80X2);
        LOG_INFO("cpu sys");
        aic_pcie_map_set(adev, 3, AIC_MAP_SCTL_BASE_AIC8800D80X2, AIC_MAP_SCTL_LIMT_AIC8800D80X2, AIC_MAP_SCTL_ADDR_AIC8800D80X2);
        LOG_INFO("share mem");
        aic_pcie_map_set(adev, 4, AIC_MAP_SHRM_BASE_AIC8800D80X2, AIC_MAP_SHRM_LIMT_AIC8800D80X2, AIC_MAP_SHRM_ADDR_AIC8800D80X2);

        adev->emb_hdma = adev->map0 + AIC_MAP_HDMA_BASE_AIC8800D80X2;
        adev->emb_tpci = adev->map0 + AIC_MAP_TPCI_BASE_AIC8800D80X2;
        adev->emb_mbox = adev->map0 + AIC_MAP_MBOX_BASE_AIC8800D80X2;
        adev->emb_sctl = adev->map0 + AIC_MAP_SCTL_BASE_AIC8800D80X2;
        adev->emb_shrm = adev->map0 + AIC_MAP_SHRM_BASE_AIC8800D80X2;
    }

#if 0
    //3. aic chip config
    addr = adev->emb_tpci + 0x50;  // msi_source: [27:16]~sw, [15:0]~emb2app_irq
    rd  = readl(addr);
    wr  = rd | 0xfffffff;
    writel(wr, addr);
    //> msi source = sw
    addr = adev->emb_tpci + 0xe8;  // [11:0] sw_msi_en
    rd  = readl(addr);
    wr  = rd | 0xfff;
    writel(wr, addr);
    //> msi source = mailbox
    addr = adev->emb_mbox + 0x214; // [ 1:0] g1_emb2app_linesel_low
    rd  = readl(addr);
    wr  = rd | 0x3;
    writel(wr, addr);

    addr = adev->emb_mbox + 0x20c; // [15:0] g1_emb2app_enable
    rd  = readl(addr);
    wr  = rd | 0xffff;
    writel(wr, addr);
#endif
    atomic_set(&adev->cnt_msi, 0);
}

int aicwf_pcie_dma(struct aic_pci_dev *adev, void *dst, void *src, u32 size, u8 dir)
{
    //> aic pcie dma api
    //! CAUTION USAGE: YOU SHOULD ONLY CALL FOR INITIALIZE(BEFORE FIRMWARE RUNNING)

    struct aicdma *adma;
    u32 k, rd, root_off, lli_off;
    u16 num=0, ctrl, lli_old, lli_tar;
    u32 aic_map_shrm_addr, aic_map_shrm_dma_off, aic_dma_mps;

    if (adev->chip_id == PRODUCT_ID_AIC8800D80) {
        aic_map_shrm_addr = AIC_MAP_SHRM_ADDR_AIC8800D80;
        aic_map_shrm_dma_off = AIC_MAP_SHRM_DMA_OFF_AIC8800D80;
        aic_dma_mps = AIC_DMA_MPS_AIC8800D80;
    } else if (adev->chip_id == PRODUCT_ID_AIC8800D80X2) {
        aic_map_shrm_addr = AIC_MAP_SHRM_ADDR_AIC8800D80X2;
        aic_map_shrm_dma_off = AIC_MAP_SHRM_DMA_OFF_AIC8800D80X2;
        aic_dma_mps = AIC_DMA_MPS_AIC8800D80X2;
    }

    LOG_INFO("    aic dma set: %08llx -> %08llx, size = %x", (u64)src, (u64)dst, size);
    adma = (struct aicdma *)(adev->emb_shrm + aic_map_shrm_dma_off);

    ctrl = 0x1F;    // count_en=1, count_no=F

    while(size > aic_dma_mps)
    {
        num++;

        adma->src   = (u32)src;
        adma->dst   = (u32)dst;
        adma->misc  = aic_dma_mps | (ctrl<<16);
        adma->next  = aic_map_shrm_addr+aic_map_shrm_dma_off + num*16;

        src  += aic_dma_mps;
        dst  += aic_dma_mps;
        size -= aic_dma_mps;
        adma += 1;
    }
    adma->src   = (u32)src;
    adma->dst   = (u32)dst;
    adma->misc  = size | (ctrl<<16);
    adma->next  = 0;
    num++;

    root_off = (dir == AIC_TRAN_DRV2EMB) ? 0x08 : 0x00; // lli_root2 : lli_root0
    lli_off  = 0xBC;    // lli_counter15
    lli_old = readl(adev->emb_hdma + lli_off);
    lli_tar = lli_old + num;
    wmb();
    writel(aic_map_shrm_addr+aic_map_shrm_dma_off, adev->emb_hdma + root_off);
    wmb();

    // tbd: modify DELAY to ACK
    k = 0;
    do {// wait for dma done
        k++;
#ifdef AIC_PCIE_API_DELAY
        udelay(50*num);
#else
        //msleep(1);
        udelay(20);
#endif
        rd = readl(adev->emb_hdma + lli_off);
        LOG_INFO("    aic dma cnt = %x", rd);
    } while((rd < lli_tar) && (k < 500 * num));

    if(rd == lli_tar)
    {
        LOG_INFO("    aic dma done, dma irq success, lli_cnt %d -> %d(%d)", lli_old, rd, lli_tar);
        return 0;
    }
    else
    {
        LOG_WARN("    aic dma done, dma irq failed!, lli_cnt %d -> %d(%d)", lli_old, rd, lli_tar);
        return -1;
    }
}

int aicwf_pcie_tran(struct aic_pci_dev *adev, void *addr_emb, void *buf, u32 blen, u8 dir, u8 rem)
{
    //> transfer data for: driver <-> firmware dir=AIC_TRAN_DRV2EMB/AIC_TRAN_EMB2DRV
    //! CAUTION USAGE: YOU SHOULD ONLY CALL FOR INITIALIZE(BEFORE FIRMWARE RUNNING)
    // rem  : need re-malloc, 1: the <buf> can't be used for dma_map directly

    void *addr_cpu;
    dma_addr_t addr_dma;
    enum dma_data_direction dma_dir;
    //u16  k;
    struct device *dev;
    u32 ret;

    dma_dir = (dir == AIC_TRAN_DRV2EMB) ? DMA_TO_DEVICE : DMA_FROM_DEVICE;

    dev = &adev->pdev->dev;

    addr_cpu = buf;
    if(rem)
    {
        addr_cpu = kmalloc(blen, GFP_KERNEL);
        if(addr_cpu == NULL)
        {
            LOG_WARN("kmalloc for test failed");
            return -1;
        }
        if(dir == AIC_TRAN_DRV2EMB)
            memcpy(addr_cpu, buf, blen);
            //for(k=0; k<wlen; k++)
            //    ((u32 *)addr_cpu)[k] = ((u32 *)buf)[k];
    }

    // 2. dma map
    addr_dma = dma_map_single(dev, addr_cpu, blen, dma_dir);
    if(dma_mapping_error(dev, addr_dma))
    {
        kfree(addr_cpu);
        LOG_WARN("dma_map_single for test failed");
        return -2;
    }

    // 3. device transfer
    if(dir == AIC_TRAN_DRV2EMB)
    {
        //<no need> dma_sync_single_for_device(dev, addr_dma, blen, dma_dir);
        ret = aicwf_pcie_dma(adev, addr_emb, (void *)addr_dma, blen, dir);
    }
    else
    {
        ret = aicwf_pcie_dma(adev, (void *)addr_dma, addr_emb, blen, dir);
#ifdef AIC_PCIE_API_DELAY
        udelay(10); // a poor way for data integrity
#else
        msleep(1);
#endif
        dma_sync_single_for_cpu(dev, addr_dma, blen, dma_dir);
        if(rem)
            memcpy(buf, addr_cpu, blen);
            //for(k=0; k<wlen; k++)
            //    ((u32 *)buf)[k] = ((u32 *)addr_cpu)[k];
    }

    dma_unmap_single(dev, addr_dma, blen, dma_dir);
    if(rem)
        kfree(addr_cpu);
    return ret;
}

int aicwf_pcie_test_dma(struct aic_pci_dev *adev, u32 off, u16 wlen, u8 dir)
{
    void *addr_cpu, *addr_emb;
    u8   *addr_emb_map;
    dma_addr_t addr_dma;
    enum dma_data_direction dma_dir;
    u32  blen, k;
    struct device *dev;
    u32  xref, xdut, ret;
    u32 aic_map_shrm_addr;

    if (adev->chip_id == PRODUCT_ID_AIC8800D80) {
        aic_map_shrm_addr = AIC_MAP_SHRM_ADDR_AIC8800D80;
    } else if (adev->chip_id == PRODUCT_ID_AIC8800D80X2) {
        aic_map_shrm_addr = AIC_MAP_SHRM_ADDR_AIC8800D80X2;
    }

    addr_emb     = (void *)aic_map_shrm_addr + off;
    addr_emb_map = adev->emb_shrm    + off;

    LOG_INFO("***** TEST TRANSFER DATA %s *****", (dir == AIC_TRAN_DRV2EMB) ? "drv2emb" : "emb2drv");

    dma_dir = (dir == AIC_TRAN_DRV2EMB) ? DMA_TO_DEVICE : DMA_FROM_DEVICE;

    dev = &adev->pdev->dev;

    blen = wlen*4;
    addr_cpu = kmalloc(blen, GFP_KERNEL);
    if(addr_cpu == NULL)
    {
        LOG_WARN("kmalloc for test failed");
        return -1;
    }

    // 1. prepare src data
    for(k=0; k<wlen; k++)
    {
        xref = (k | (k<<8) | (k<<16) | (k<<24));
        if(dir == AIC_TRAN_DRV2EMB)
            ((u32 *)addr_cpu)[k] = xref;
        else
            writel(xref, addr_emb_map + k*4);
    }
    wmb();

    // 2. dma map
    addr_dma = dma_map_single(dev, addr_cpu, blen, dma_dir);
    if(dma_mapping_error(dev, addr_dma))
    {
        kfree(addr_cpu);
        LOG_WARN("dma_map_single for test failed");
        return -2;
    }

    // 3. device transfer
    if(dir == AIC_TRAN_DRV2EMB)
    {
        //<no need> dma_sync_single_for_device(dev, addr_dma, blen, dma_dir);
        ret = aicwf_pcie_dma(adev, addr_emb, (void *)addr_dma, blen, dir);
    }
    else
    {
        ret = aicwf_pcie_dma(adev, (void *)addr_dma, addr_emb, blen, dir);
#ifdef AIC_PCIE_API_DELAY
        udelay(10); // a poor way for data integrity
#else
        msleep(1);
#endif
        dma_sync_single_for_cpu(dev, addr_dma, blen, dma_dir);
    }

    // 4. verify transfer
    if(ret == 0)
    {
        for(k=0; k<wlen; k++)
        {
            xref = (k | (k<<8) | (k<<16) | (k<<24));

            xdut = (dir == AIC_TRAN_DRV2EMB) ? readl(addr_emb_map + k*4) : ((u32 *)addr_cpu)[k];
            if(xref != xdut)
            {
                LOG_WARN("FAIL itran[%x], %x, %x", k, xref, xdut);
                ret = -3;
                break;
            }
        }
    }

    dma_unmap_single(dev, addr_dma, blen, dma_dir);
    kfree(addr_cpu);
    return ret;
}

int aicwf_pcie_test_accs_emb(struct aic_pci_dev *adev)
{
    u8 *addr;
    u32 rd0, rd1, rd2;

    LOG_INFO("***** TEST ACCESS EMB *****");
    addr = adev->emb_hdma + 0xc0;  // [ 3:0] oft_readback_addr
    rd0 = readl(addr);
    writel(  0, addr); rd1 = readl(addr);
    writel( -1, addr); rd2 = readl(addr);
    writel(rd0, addr);
    LOG_INFO("\tTEST host dma: %08x, %08x, %08x", rd0, rd1, rd2);
    if((rd1 != 0) || (rd2 != 0xF))
    {
        LOG_ERROR("\tERROR");
        return -1;
    }

    addr = adev->emb_tpci + 0x0fc;  // [31:0] rg_reserved_3
    rd0 = readl(addr);
    writel(  0, addr); rd1 = readl(addr);
    writel( -1, addr); rd2 = readl(addr);
    writel(rd0, addr);
    LOG_INFO("\tTEST top pcie: %08x, %08x, %08x", rd0, rd1, rd2);
    if((rd1 != 0) || (rd2 !=  -1))
    {
        LOG_ERROR("\tERROR");
        return -2;
    }

    addr = adev->emb_mbox + 0x29c;  // [31:0] g1_comreg7
    rd0 = readl(addr);
    writel(  0, addr); rd1 = readl(addr);
    writel( -1, addr); rd2 = readl(addr);
    writel(rd0, addr);
    LOG_INFO("\tTEST mail box: %08x, %08x, %08x", rd0, rd1, rd2);
    if((rd1 != 0) || (rd2 !=  -1))
    {
        LOG_ERROR("\tERROR");
        return -3;
    }

    addr = adev->emb_sctl + 0x040;  // [15:0] tports_sel
    rd0 = readl(addr);
    writel(  0, addr); rd1 = readl(addr);
    writel( -1, addr); rd2 = readl(addr);
    writel(rd0, addr);
    LOG_INFO("\tTEST sys ctrl: %08x, %08x, %08x", rd0, rd1, rd2);
    if((rd1 != 0) || (rd2 != 0xFFFF))
    {
        LOG_ERROR("\tERROR");
        return -4;
    }

    addr = adev->emb_shrm + 0x7f0;  // [31:0] 0x1a07f0
    rd0 = readl(addr);
    writel(  0, addr); rd1 = readl(addr);
    writel( -1, addr); rd2 = readl(addr);
    writel(rd0, addr);
    LOG_INFO("\tTEST shar mem: %08x, %08x, %08x", rd0, rd1, rd2);
    if((rd1 != 0) || (rd2 != -1))
    {
        LOG_ERROR("\tERROR");
        return -5;
    }
    return 0;
}

int aicwf_pcie_test_msi(struct aic_pci_dev *adev)
{
    u8 *addr;
    u32 n, k;
    atomic_t *cnt = &adev->cnt_msi;

    LOG_INFO("***** TEST MSI by sw_msi *****");
    addr = adev->emb_tpci + 0xec;  // sw_msi set [11:0]
    writel(0xfff, addr);
    addr = adev->emb_tpci + 0x6c;  // sw_msi clr [27:16]
    writel(0xfff0000, addr);

    k = 0;
    do {// wait for msi irq
        k++;
#ifdef AIC_PCIE_API_DELAY
        mdelay(10);
#else
        msleep(1);
#endif
        n = atomic_read(cnt);
        LOG_INFO("    %d: msi cnt = %d", k, n);
    } while((n < 12) && (k < 10));
    if(n < 12)
    {
        LOG_WARN(" MSI IRQ num < MSI TRIG num");
    }

    LOG_INFO("***** TEST MSI by emb2app *****");
    addr = adev->emb_mbox + 0x300; // g1_emb2app set
    writel(0xffff, addr);
    addr = adev->emb_mbox + 0x208; // g1_emb2app clr
    writel(0xffff, addr);

    k = 0;
    do {// wait for msi irq
        k++;
#ifdef AIC_PCIE_API_DELAY
        mdelay(10);
#else
        msleep(1);
#endif
        n = atomic_read(cnt);
        LOG_INFO("    %d: msi cnt = %d", k, n);
    } while((n < 28) && (k < 10));
    if(n < 28)
    {
        LOG_WARN(" MSI IRQ num < MSI TRIG num");
        return -1;
    }
    return 0;
}

#if 0
irqreturn_t aic_pcie_irq_hdr(int irq, void *dev_id)
{
    //x disable_irq_nosync(irq);
    LOG_INFO("msi irq: %d", atomic_read(&adev->cnt_msi));
    atomic_inc(&adev->cnt_msi);
    //x enable_irq(irq);
    return IRQ_HANDLED;
}
#endif

int aicwf_pcie_setst(struct aic_pci_dev *adev)
{
    int ret;

    // 1. hardware initial config
    aicwf_pcie_cfg(adev);

    // 2. test pcie
    ret = aicwf_pcie_test_accs_emb(adev);
    if(ret)
    {
        LOG_INFO("FAILED TEST: aic_pcie_test_accs_emb %d", ret);
        return ret;
    }

    ret = aicwf_pcie_test_dma(adev, AIC_TEST_TRAN_OFF0, AIC_TEST_TRAN_WLEN, AIC_TRAN_DRV2EMB);
    if(ret)
    {
        LOG_INFO("FAILED TEST: aic_pcie_test_accs_emb %d", ret);
        return ret;
    }

    ret = aicwf_pcie_test_dma(adev, AIC_TEST_TRAN_OFF1, AIC_TEST_TRAN_WLEN, AIC_TRAN_EMB2DRV);
    if(ret)
    {
        LOG_INFO("FAILED TEST: aic_pcie_test_accs_emb %d", ret);
        return ret;
    }

    return ret;
}

