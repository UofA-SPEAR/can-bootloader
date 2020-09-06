
# make targets for flashing with openocd

.PHONY: flash
flash: all
	openocd -f board/st_nucleo_f3.cfg -c "program $(PROJNAME).elf verify reset" -c "shutdown"

.PHONY: reset
reset:
	openocd -f board/st_nucleo_f3.cfg -c "init" -c "reset" -c "shutdown"
