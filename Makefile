CC = clang
OPT = -Iinclude

SRC = ${wildcard src/*.c}
HDR = ${wildcard include/*.h}

TEST_SRC = ${filter-out ${wildcard test/*.*.*},${wildcard test/*.c}}
TEST_HDR = ${wildcard test/*.h}


%.yy.c: %.flex $(TEST_HDR)
	flex -o $@ $<

%.gg.c: %.g bin/parvum_parse
	bin/parvum_parse $<

bin/parvum_parse: $(SRC) $(HDR)
	$(CC) $(OPT) $(SRC) -o $@

bin/math_parser: test/lexer_math.yy.c test/grammar_math.gg.c $(TEST_SRC) $(TEST_HDR) 
	$(CC) test/lexer_math.yy.c test/grammar_math.gg.c $(TEST_SRC) -o $@

clean:
	rm -f test/*.*.*
	rm -f bin/*

.PHONY: clean
