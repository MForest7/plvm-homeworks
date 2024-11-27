SHELL:=/bin/bash

TESTS=$(sort $(basename $(wildcard *.lama)))


LAMAC=lamac
INTERPRETER=../interpreter/interpreter

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo $@
	@$(LAMAC) -b $@.lama
	@echo 'My interpreter'
	@time $(INTERPRETER) $@.bc
	@echo ''
	@echo 'Source code interpreter'
	@time cat Sort.input | $(LAMAC) -i $<
	@echo ''
	@echo 'Stack machine'
	@time cat Sort.input | $(LAMAC) -s $<

clean:
	$(RM) test*.log *.s *~ $(TESTS) *.i *.bc