; crt0.s - NES start-up, NMI handler (sprite DMA + VRAM update list), iNES header, vectors, CHR-ROM.
.export __STARTUP__:absolute=1
.export _nmi_ready, _nmi_count, _ppumask, _ppuctrl, _vlen, _vbuf, _oam
.export _nt_rom, _attr_rom
.import _main, copydata
.import __STACKSTART__, __STACKSIZE__
.importzp sp
.segment "HEADER"
  .byte "NES",$1A,2,1,0,0          ; 2 x 16 KB PRG, 1 x 8 KB CHR, mapper 0 (NROM), horizontal mirroring
.segment "ZEROPAGE"
_nmi_ready: .res 1                 ; main sets it when OAM / _vbuf are complete; the NMI clears it
_nmi_count: .res 1
_ppumask:   .res 1
_ppuctrl:   .res 1
_vlen:      .res 1
.segment "OAM"
_oam:       .res 256
.segment "BSS"
_vbuf:      .res 56                ; VRAM update list: [addr hi][addr lo][count][data...] ...
.segment "STARTUP"
reset:
  sei
  cld
  ldx #$40
  stx $4017
  ldx #$FF
  txs
  inx
  stx $2000
  stx $2001
  stx $4010
  stx $4015
: bit $2002
  bpl :-
  lda #0
  tax
: sta $0000,x
  sta $0100,x
  sta $0300,x
  sta $0400,x
  sta $0500,x
  sta $0600,x
  sta $0700,x
  inx
  bne :-
  lda #$FF
: sta $0200,x                      ; all sprites off-screen
  inx
  bne :-
: bit $2002
  bpl :-
  lda #<(__STACKSTART__+__STACKSIZE__)
  sta sp
  lda #>(__STACKSTART__+__STACKSIZE__)
  sta sp+1
  jsr copydata
  jsr _main
: jmp :-
nmi:
  pha
  txa
  pha
  tya
  pha
  lda _nmi_ready
  beq @skip
  lda #0
  sta $2003
  lda #>_oam
  sta $4014                        ; sprite DMA (513 cycles)
  ldy #0
@next:
  cpy _vlen
  bcs @done
  lda _vbuf,y
  sta $2006
  iny
  lda _vbuf,y
  sta $2006
  iny
  ldx _vbuf,y
  iny
@copy:
  lda _vbuf,y
  sta $2007
  iny
  dex
  bne @copy
  beq @next
@done:
  lda #0
  sta _vlen
  sta _nmi_ready
  lda _ppuctrl
  sta $2000
  lda #0
  sta $2005
  sta $2005
  lda _ppumask
  sta $2001
@skip:
  inc _nmi_count
  pla
  tay
  pla
  tax
  pla
  rti
irq:
  rti
.segment "RODATA"
_nt_rom:   .incbin "build/nt.bin"
_attr_rom: .incbin "build/attr.bin"
.segment "VECTORS"
  .word nmi, reset, irq
.segment "CHARS"
  .incbin "build/chr.bin"
