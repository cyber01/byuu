auto VDC::hpulse() -> void {
  timing.hstate = HDS;
  timing.hoffset = 0;
}

auto VDC::vpulse() -> void {
  timing.vstate = VSW;
  timing.voffset = 0;
}

auto VDC::hclock() -> void {
  background.scanline(timing.voffset);
  sprite.scanline(timing.voffset);

  if(timing.vstate == VDW) {
    background.render(timing.voffset);
    sprite.render(timing.voffset);

    for(uint x : range(vdp.vce.width())) {
      output[x] = 0;
      if(sprite.output[x].color && sprite.output[x].priority) {
        output[x] = sprite.output[x].color << 0 | sprite.output[x].palette << 4 | 1 << 8;
      } else if(background.output[x].color) {
        output[x] = background.output[x].color << 0 | background.output[x].palette << 4 | 0 << 8;
      } else if(sprite.output[x].color) {
        output[x] = sprite.output[x].color << 0 | sprite.output[x].palette << 4 | 1 << 8;
      }
    }
  }

  if(timing.coincidence++ == io.coincidence) irq.raise(IRQ::Line::Coincidence);
}

auto VDC::vclock() -> void {
  timing.voffset++;
  switch(timing.vstate) {
  case VSW:
    if(timing.voffset == timing.verticalSyncWidth + 1) {
      timing.vstate = VDS;
      timing.voffset = 0;
    } break;
  case VDS:
    if(timing.voffset == timing.verticalDisplayStart) {
      timing.vstate = VDW;
      timing.voffset = 0;
      timing.coincidence = 64;
    } break;
  case VDW:
    if(timing.voffset == timing.verticalDisplayWidth + 1) {
      timing.vstate = VCR;
      timing.voffset = 0;
      irq.raise(IRQ::Line::Vblank);
      dma.satbStart();
    } break;
  case VCR:
    if(timing.voffset == timing.verticalDisplayEnd) {
      timing.vstate = VSW;
      timing.voffset = 0;
    } break;
  }
}

auto VDC::read(uint2 address) -> uint8 {
  uint8 data = 0x00;
  uint1 a0 = address.bit(0);
  uint1 a1 = address.bit(1);

  if(a1 == 0) {
    //SR
    if(a0 == 1) return data;
    data.bit(0) = irq.collision.pending;
    data.bit(1) = irq.overflow.pending;
    data.bit(2) = irq.coincidence.pending;
    data.bit(3) = irq.transferSATB.pending;
    data.bit(4) = irq.transferVRAM.pending;
    data.bit(5) = irq.vblank.pending;
    irq.lower();
    return data;
  }

  if(io.address == 0x02) {
    //VRR
    data = vram.dataRead.byte(a0);
    if(a0 == 1) {
      vram.addressRead += vram.addressIncrement;
      vram.dataRead = vram.read(vram.addressRead);
    }
    return data;
  }

  return data;
}

auto VDC::write(uint2 address, uint8 data) -> void {
  uint1 a0 = address.bit(0);
  uint1 a1 = address.bit(1);

  if(a1 == 0) {
    //AR
    if(a0 == 0) io.address = data.bit(0,4);
    return;
  }

  if(io.address == 0x00) {
    //MAWR
    vram.addressWrite.byte(a0) = data;
    return;
  }

  if(io.address == 0x01) {
    //MARR
    vram.addressRead.byte(a0) = data;
    vram.dataRead = vram.read(vram.addressRead);
    return;
  }

  if(io.address == 0x02) {
    //VWR
    vram.dataWrite.byte(a0) = data;
    if(a0 == 1) {
      vram.write(vram.addressWrite, vram.dataWrite);
      vram.addressWrite += vram.addressIncrement;
    }
    return;
  }

  if(io.address == 0x05) {
    //CR
    if(a0 == 0) {
      irq.collision.enable   = data.bit(0);
      irq.overflow.enable    = data.bit(1);
      irq.coincidence.enable = data.bit(2);
      irq.vblank.enable      = data.bit(3);
      io.externalSync        = data.bit(4,5);
      sprite.enable          = data.bit(6);
      background.enable      = data.bit(7);
    }
    if(a0 == 1) {
      io.displayOutput = data.bit(0,1);
      io.dramRefresh   = data.bit(2);
      if(data.bit(3,4) == 0) vram.addressIncrement = 0x01;
      if(data.bit(3,4) == 1) vram.addressIncrement = 0x20;
      if(data.bit(3,4) == 2) vram.addressIncrement = 0x40;
      if(data.bit(3,4) == 3) vram.addressIncrement = 0x80;
    }
    return;
  }

  if(io.address == 0x06) {
    //RCR
    if(a0 == 0) io.coincidence.bit(0,7) = data.bit(0,7);
    if(a0 == 1) io.coincidence.bit(8,9) = data.bit(0,1);
    return;
  }

  if(io.address == 0x07) {
    //BXR
    if(a0 == 0) background.hscroll.bit(0,7) = data.bit(0,7);
    if(a0 == 1) background.hscroll.bit(8,9) = data.bit(0,1);
    return;
  }

  if(io.address == 0x08) {
    //BYR
    if(a0 == 0) background.vscroll.bit(0,7) = data.bit(0,7);
    if(a0 == 1) background.vscroll.bit(8)   = data.bit(0);
    background.vcounter = background.vscroll;  //updated on both even and odd writes
    return;
  }

  if(io.address == 0x09) {
    //MWR
    if(a0 == 1) return;
    io.vramAccess   = data.bit(0,1);
    io.spriteAccess = data.bit(2,3);
    if(data.bit(4,5) == 0) background.width =  32;
    if(data.bit(4,5) == 1) background.width =  64;
    if(data.bit(4,5) == 2) background.width = 128;
    if(data.bit(4,5) == 3) background.width = 128;
    if(data.bit(6) == 0) background.height = 32;
    if(data.bit(6) == 1) background.height = 64;
    io.cgMode = data.bit(7);
    return;
  }

  if(io.address == 0x0a) {
    //HSR
    if(a0 == 0) timing.horizontalSyncWidth    = data.bit(0,4);
    if(a0 == 1) timing.horizontalDisplayStart = data.bit(0,6);
    return;
  }

  if(io.address == 0x0b) {
    //HDR
    if(a0 == 0) timing.horizontalDisplayWidth = data.bit(0,6);
    if(a0 == 1) timing.horizontalDisplayEnd   = data.bit(0,6);
    return;
  }

  if(io.address == 0x0c) {
    //VPR
    if(a0 == 0) timing.verticalSyncWidth    = data.bit(0,4);
    if(a0 == 1) timing.verticalDisplayStart = data.bit(0,7);
    return;
  }

  if(io.address == 0x0d) {
    //VDR
    if(a0 == 0) timing.verticalDisplayWidth.bit(0,7) = data.bit(0,7);
    if(a0 == 1) timing.verticalDisplayWidth.bit(8)   = data.bit(0);
    return;
  }

  if(io.address == 0x0e) {
    //VCR
    if(a0 == 0) timing.verticalDisplayEnd = data.bit(0,7);
    return;
  }

  if(io.address == 0x0f) {
    //DCR
    if(a0 == 1) return;
    irq.transferSATB.enable = data.bit(0);
    irq.transferVRAM.enable = data.bit(1);
    dma.sourceIncrementMode = data.bit(2);
    dma.targetIncrementMode = data.bit(3);
    dma.satbRepeat          = data.bit(4);
    return;
  }

  if(io.address == 0x10) {
    //SOUR
    dma.source.byte(a0) = data;
    return;
  }

  if(io.address == 0x11) {
    //DESR
    dma.target.byte(a0) = data;
    return;
  }

  if(io.address == 0x12) {
    //LENR
    dma.length.byte(a0) = data;
    if(a0 == 1) dma.vramStart();
    return;
  }

  if(io.address == 0x13) {
    //DVSSR
    dma.satbSource.byte(a0) = data;
    if(a0 == 1) dma.satbQueue();
    return;
  }
}

auto VDC::power() -> void {
  for(auto& data : vram.memory) data = 0;
  vram.addressRead = 0;
  vram.addressWrite = 0;
  vram.addressIncrement = 0;
  vram.dataRead = 0;
  vram.dataWrite = 0;
  satb = {};
  irq = {};
  dma = {};
  timing = {};
  background = {};
  sprite = {};

  dma.vdc = *this;
  background.vdc = *this;
  sprite.vdc = *this;
}

auto VDC::VRAM::read(uint16 address) const -> uint16 {
  if(address.bit(15)) return 0x0000;  //todo: random data?
  return memory[address];
}

auto VDC::VRAM::write(uint16 address, uint16 data) -> void {
  if(address.bit(15)) return;
  memory[address] = data;
}

auto VDC::SATB::read(uint8 address) const -> uint16 {
  return memory[address];
}

auto VDC::SATB::write(uint8 address, uint16 data) -> void {
  memory[address] = data;
}
