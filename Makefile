DIRS=ppu spu
include ${CELL_TOP}/buildutils/make.footer

run:
	./ppu/ppu_mailbox input/23.ppm output/23_out.ppm 2 4 8 10 10
