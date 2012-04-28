DIRS=ppu spu
include ${CELL_TOP}/buildutils/make.footer

run: run_first run_second run_3 run_4

run_first:
	./ppu/ppu_mailbox input/3.ppm output/3_out.ppm 2 4 8 10 10

run_second:
	./ppu/ppu_mailbox input/23.ppm output/23_out.ppm 2 4 8 10 10

run_3:
	./ppu/ppu_mailbox input/23.ppm output/23_out.ppm 2 4 8 10 10

run_4:
	./ppu/ppu_mailbox input/23.ppm output/23_out.ppm 2 4 8 10 10
