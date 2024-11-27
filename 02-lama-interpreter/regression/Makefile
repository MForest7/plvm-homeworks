TESTS=$(sort $(basename $(wildcard test*.lama)))

LAMAC=lamac
INTERPRETER=../interpreter/interpreter
BCDUMP=../interpreter/dump

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo "regression/$@"
	@LAMA=../runtime $(LAMAC) -b $< && cat $@.input | $(INTERPRETER) $@.bc > $@.log && diff $@.log orig/$@.log

clean:
	$(RM) test*.log *.s *.sm *~ $(TESTS) *.i *.bc
	$(MAKE) clean -C expressions
	$(MAKE) clean -C deep-expressions
