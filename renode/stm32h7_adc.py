if request.IsInit:
    regs = {}
    adc_value = 2048
elif request.IsWrite:
    off = request.Offset
    val = request.Value
    if off == 0x08:
        ADCAL    = (1 << 31)
        DEEPPWD  = (1 << 29)
        ADVREGEN = (1 << 28)
        ADEN     = (1 << 0)
        ADDIS    = (1 << 1)
        ADSTART  = (1 << 2)
        ADSTP    = (1 << 4)
        cr = val
        isr = regs.get(0x00, 0)
        if cr & ADCAL:
            cr = cr & ~ADCAL
        if cr & DEEPPWD:
            isr = isr & ~((1 << 12) | (1 << 0))
        else:
            isr = isr | (1 << 12)
        if (cr & ADEN) and not (cr & DEEPPWD) and (cr & ADVREGEN):
            isr = isr | (1 << 0)
        if cr & ADDIS:
            cr = cr & ~(ADEN | ADDIS)
            isr = isr & ~(1 << 0)
        if cr & ADSTART:
            isr = isr | (1 << 2) | (1 << 3)
            cr = cr & ~ADSTART
        if cr & ADSTP:
            cr = cr & ~ADSTP
        regs[0x00] = isr
        regs[0x08] = cr
    else:
        regs[off] = val
elif request.IsRead:
    off = request.Offset
    if off == 0x40:
        request.Value = adc_value
        isr = regs.get(0x00, 0)
        isr = isr & ~((1 << 2) | (1 << 3))
        regs[0x00] = isr
    else:
        request.Value = regs.get(off, 0)
