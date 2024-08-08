  .section .text.enter
  .global loader_entry, loader_ret

loader_entry:
  call loader_init

loader_ret:
  mv gp, a0
  mv sp, a1
  lw ra, 0(sp)
  lw t0, 4(sp)
  lw t1, 8(sp)
  lw t2, 12(sp)
  lw t3, 16(sp)
  lw t4, 20(sp)
  lw t5, 24(sp)
  lw t6, 28(sp)
  lw a0, 32(sp)
  lw a1, 36(sp)
  lw a2, 40(sp)
  lw a3, 44(sp)
  lw a4, 48(sp)
  lw a5, 52(sp)
  lw a6, 56(sp)
  lw a7, 60(sp)
  lw s0, 64(sp)
  lw s1, 68(sp)
  lw s2, 72(sp)
  lw s3, 76(sp)
  lw s4, 80(sp)
  lw s5, 84(sp)
  lw s6, 88(sp)
  lw s7, 92(sp)
  lw s8, 96(sp)
  lw s9, 100(sp)
  lw s10, 104(sp)
  lw s11, 108(sp)
  lw sp, 112(sp)
  jr gp
