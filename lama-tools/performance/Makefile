SHELL:=/bin/bash

TESTS=$(sort $(basename $(wildcard *.lama)))


LAMAC=lamac
INTERPRETER=../tools/build/bin/interpreter

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo "Test: $@"
	@echo ''
	@$(LAMAC) -b $@.lama
	@echo 'Bytecode interpreter'
	@time $(INTERPRETER) $@.bc
	@echo ''
	@echo 'Original source code interpreter'
	@time cat Sort.input | $(LAMAC) -i $<
	@echo ''
	@echo 'Original bytecode interpreter'
	@time cat Sort.input | $(LAMAC) -s $<

clean:
	$(RM) test*.log *.s *~ $(TESTS) *.i *.bc