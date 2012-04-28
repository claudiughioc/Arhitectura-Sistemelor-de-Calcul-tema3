DIRS=ppu spu
include ${CELL_TOP}/buildutils/make.footer

run:
	./ppu/ppu_mailbox input/3.ppm output/3_out.ppm 2 4 8 10 10
