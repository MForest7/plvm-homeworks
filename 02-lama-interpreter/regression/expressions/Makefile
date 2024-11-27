TESTS=$(sort $(basename $(wildcard generated*.lama)))

LAMAC=lamac
INTERPRETER=../../interpreter/interpreter
BCDUMP=../../interpreter/dump

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo "regression/expressions/$@"
	@LAMA=../../runtime $(LAMAC) -b $< && cat $@.input | $(INTERPRETER) $@.bc > $@.log && diff $@.log orig/$@.log

clean:
	rm -f *.log *.s *~ *.bcd *.bc
	find . -maxdepth 1 -type f -not -name '*.*' -not -name 'Makefile' -delete
