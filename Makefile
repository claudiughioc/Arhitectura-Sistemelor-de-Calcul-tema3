DIRS=ppu spu
include ${CELL_TOP}/buildutils/make.footer

run_first:
	./ppu/ppu_mailbox input/3.ppm output/3_out.ppm 2 4 8 10 10

run_second:
	./ppu/ppu_mailbox input/23.ppm output/23_out.ppm 2 4 8 10 10
