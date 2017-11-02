#
# cleanup targets:
################################################################################

.PHONY: all
all: $(PROJNAME).bin $(PROJNAME).lst $(PROJNAME).size.txt Makefile
	$(PRINT) "> done: $(PROJNAME)"
	@ $(SZ) $(PROJNAME).elf

.PHONY: clean
clean:
	$(Q)-rm -f $(OBJS)
	$(Q)-rm -f $(OBJS:.o=.lst)
	$(Q)-rm -f $(OBJS:.o=.d)
	$(Q)-rm -f $(PROJNAME).elf
	$(Q)-rm -f $(PROJNAME).bin
	$(Q)-rm -f $(PROJNAME).lst
	$(Q)-rm -f $(PROJNAME).map
	$(Q)-rm -f $(PROJNAME).size.txt

.PHONY: rebuild
rebuild: clean all

#
# file targets:
################################################################################

# linked elf-object
$(PROJNAME).elf: $(OBJS) $(LDSCRIPT)
	$(PRINT) "> linking"
	$(Q) $(LD) -o $(PROJNAME).elf $(OBJS) $(LFLAGS)

# binary
$(PROJNAME).bin: $(PROJNAME).elf
	$(PRINT) "> copying"
	$(Q) $(OC) -Obinary -j .text -j .rodata -j .data $(PROJNAME).elf $(PROJNAME).bin

# assembly listing
$(PROJNAME).lst: $(PROJNAME).elf
	$(PRINT) "> generating assembly listing"
	$(Q) $(OD) -D -h $(PROJNAME).elf > $(PROJNAME).lst

# object from c
$(COBJS): $(BUILD_DIR)/%.o : $(PROJ_ROOT)/%.c Makefile
	$(Q) $(MKDIR) $(shell dirname ${@})
	$(PRINT) "> compiling ("$<")"
	$(Q) $(CC) $(CCFLAGS) -Wa,-ahlms=$(@:.o=.lst) -o ${@} -c ${<}

# object from asm
$(ASMOBJS): $(BUILD_DIR)/%.o : $(PROJ_ROOT)/%.s Makefile
	$(Q) $(MKDIR) $(shell dirname ${@})
	$(PRINT) "> assembling ("$<")"
	$(Q) $(AS) $(ASMFLAGS) -c ${<} -o ${@}

# object from c++
$(CXXOBJS): $(BUILD_DIR)/%.o : $(PROJ_ROOT)/%.cpp Makefile
	$(Q) $(MKDIR) $(shell dirname ${@})
	$(PRINT) "> compiling ("$<")"
	$(Q) $(CXX) $(CXXFLAGS) -Wa,-ahlms=$(@:.o=.lst) -o ${@} -c ${<}

# space usage
$(PROJNAME).size.txt: $(PROJNAME).elf
	$(PRINT) "> calculating space usage"
	$(Q)$(SZ) $(PROJNAME).elf > $(PROJNAME).size.txt
	$(Q)$(NM) --size-sort --print-size -S $(PROJNAME).elf >> $(PROJNAME).size.txt

# include the dependencies for all objects
# (generated by the -MD compiler-flag)
# -include $(OBJS:.o=.d)
