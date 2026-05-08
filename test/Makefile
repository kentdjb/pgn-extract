# Makefile to run a battery of tests of the basic functionality of pgn-extract.
# Copyright (C) David J. Barnes, 2013-2026
#
# Each test is run and then, where relevant, a comparison is made between
# the output files generated in the current directory and the 'oracle'
# files that exist in $(OUTPUT).
#
# Beware of case-insensitive filenames when testing flags that
# differ only in letter case. In general, double letters have been
# used for output-files generated from using upper-case flag letters.

# Customise these for different OSs.
SEP=/
RM=rm -f
CMP=cmp

# Possible Windows settings
#SEP=\\
#RM=del
#CMP=diff

PGN_EXTRACT=..$(SEP)build$(SEP)bin$(SEP)pgn-extract
# Use for memory checking if valgrind is installed.
#PGN_EXTRACT=valgrind --dsymutil=yes ..$(SEP)pgn-extract
#PGN_EXTRACT=valgrind --dsymutil=yes --leak-check=full ..$(SEP)pgn-extract

# Location of the file of ECO classifications.
ECO_FILE=..$(SEP)eco.pgn

# Location of the input files
INPUT=infiles
# Location of the oracle output files
OUTPUT=outfiles

# Test everything.
all: no-flags-1 no-flags-2 test-7 test-append test-AA test-b test-p \
     test-checkfile test-nocomments test-duplicates \
     test-noduplicates test-e test-EE test-f test-FF test-h test-l \
     test-LL test-checkmate test-n test-NN test-output test-PP test-r \
     test-RR test-s test-SS test-t test-TT test-nounique test-v \
     test-VV test-linelength test-WW test-x test-ZZ test-y test-z test-hash \
     test-evaluation test-fencomments test-markmatches test-nochecks \
     test-nomovenumbers test-noresults test-notags test-plylimit \
     test-stalemate test-long-line test-plycount test-addhashcode \
     test-selectonly test-fifty test-repetition test-promotion \
     test-fixresulttags test-fuzzydepth test-setup test-stopafter \
     test-skipmatching test-splitvariants test-nobadresults test-allownullmoves \
     test-matchplylimit test-nestedcomments test-FENPattern test-dropply \
     test-startply test-linenumbers test-seventyfive test-elodiff test-suppressmatched \
     test-reltags test-ratedwins test-odds

# BEWARE: This removes all PGN files in the current directory.
# The required test PGN files are assumed to be in $(INPUT).
clean:
	-$(RM) *.pgn *og.txt

# No flags:
#     + No input file.
#     - Input file(s): N$(SEP)A
#     - Example input:
#       f3 e5 g4 Qh4 0-1
#     - Resulting output should be that the game input on standard input is
#       formatted as PGN on standard output.
#     - Expected output: 
#       Contents of fools-mate.pgn on standard output.
no-flags-1:
	echo "test: No input file."
	$(PGN_EXTRACT) --quiet < $(INPUT)$(SEP)fools-mate.txt

# No flags:
#     + Single input file
#     - Input file(s): fools-mate.txt
#     - Resulting output should be the input formatted as PGN on standard output.
#     - Expected output: Contents of fools-mate.pgn should appear on standard output.
no-flags-2:
	echo "test: No flags."
	$(PGN_EXTRACT) --quiet $(INPUT)$(SEP)fools-mate.txt

# -7 $(SEP) --seven
#     + Input file with games having tags additional to the seven tag roster.
#     - Input file(s): test-7.pgn
#     - Game output should have only the tags of the seven tag roster.
#     - Expected output: test-7-out.pgn
test-7:
	echo "test-7:"
	$(PGN_EXTRACT) -7 -otest-7-out.pgn --quiet $(INPUT)$(SEP)test-7.pgn
	$(CMP) test-7-out.pgn $(OUTPUT)$(SEP)test-7-out.pgn

# -a $(SEP) --append
#     + Input file containing games in any accepted format.
#     - Input file(s): test-a.txt
#     - Resulting output should contain two versions of the input game
#       formatted in PGN. The --output command is run first to create
#       a new file, then the -a version to append to that file.
#     - Expected output: test-a-out.pgn
test-append:
	echo "test-append:"
	$(PGN_EXTRACT) --output test-a-out.pgn --quiet $(INPUT)$(SEP)test-a.txt
	$(PGN_EXTRACT) -atest-a-out.pgn --quiet $(INPUT)$(SEP)test-a.txt
	$(CMP) test-a-out.pgn $(OUTPUT)$(SEP)test-a-out.pgn

	$(PGN_EXTRACT) --output test-a-out.pgn --quiet $(INPUT)$(SEP)test-a.txt
	$(PGN_EXTRACT) --append test-a-out.pgn --quiet $(INPUT)$(SEP)test-a.txt
	$(CMP) test-a-out.pgn $(OUTPUT)$(SEP)test-a-out.pgn

# -A
#     + Input file containing games and a file of arguments.
#     - Input file(s): fischer.pgn, petrosian.pgn, arglist.txt
#     - Resulting output should be files separating the unique and
#       duplicated games in the input files.
#     - Expected output: test-AA-unique.pgn, test-AA-dupes.pgn
test-AA:
	echo "test-AA:"
	$(PGN_EXTRACT) -C -A$(INPUT)$(SEP)argslist.txt --quiet
	$(CMP) test-AA-dupes.pgn $(OUTPUT)$(SEP)test-AA-dupes.pgn
	$(CMP) test-AA-unique.pgn $(OUTPUT)$(SEP)test-AA-unique.pgn

# -b
#     + Input file containing games of different length.
#     - Input file(s): fischer.pgn
#     - Resulting output: games whose number of moves is within
#       the specified bounds.
# 
test-b: test-bl45 test-bu45 test-bu30

# -p
#     + Input file containing games of different length.
#     - Input file(s): fischer.pgn
#     - Resulting output: games whose number of ply is within
#       the specified bounds.
# 
test-p: test-pl90 test-pu90 test-pu60

#     - Expected output: 45 moves or more: test-bl45-out.pgn
test-bl45:
	echo "test-b:"
	$(PGN_EXTRACT) -bl45 -otest-bl45-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-bl45-out.pgn $(OUTPUT)$(SEP)test-bl45-out.pgn

#     - Expected output: 45 moves or less: test-bu45-out.pgn
test-bu45:
	echo "test-b:"
	$(PGN_EXTRACT) -bu45 -otest-bu45-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-bu45-out.pgn $(OUTPUT)$(SEP)test-bu45-out.pgn

#     - Expected output: exactly 30 moves: test-b30-out.pgn
test-bu30:
	echo "test-b:"
	$(PGN_EXTRACT) -b30 -otest-b30-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-b30-out.pgn $(OUTPUT)$(SEP)test-b30-out.pgn

#     - Expected output: 90 moves or more: test-pl90-out.pgn
test-pl90:
	echo "test-p:"
	$(PGN_EXTRACT) -pl90 -otest-pl90-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-pl90-out.pgn $(OUTPUT)$(SEP)test-pl90-out.pgn

#     - Expected output: 90 moves or less: test-pu90-out.pgn
test-pu90:
	echo "test-p:"
	$(PGN_EXTRACT) -pu90 -otest-pu90-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-pu90-out.pgn $(OUTPUT)$(SEP)test-pu90-out.pgn

#     - Expected output: exactly 60 moves: test-p60-out.pgn
test-pu60:
	echo "test-p:"
	$(PGN_EXTRACT) -p60 -otest-p60-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-p60-out.pgn $(OUTPUT)$(SEP)test-p60-out.pgn

# 
# -c $(SEP) --checkfile
#     + Input files containing games.
#     - Input file(s): fischer.pgn, petrosian.pgn
#     - Resulting output should contain matched games that do not already occur in
#       the check file.
#     - Expected output: test-c-out.pgn
test-checkfile:
	echo "test-checkfile:"
	$(PGN_EXTRACT) -c$(INPUT)$(SEP)petrosian.pgn -D -TpPetrosian -otest-c-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-c-out.pgn $(OUTPUT)$(SEP)test-c-out.pgn
	$(PGN_EXTRACT) -c$(INPUT)$(SEP)clist.txt -D -TpPetrosian -otest-c-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-c-out.pgn $(OUTPUT)$(SEP)test-c-out.pgn

# -C $(SEP) --nocomments
#     + Input file containing games with comments
#     - Input file(s): test-C.pgn
#     - Resulting output should have all comments removed.
#     - Expected output: test-CC-out.pgn
test-nocomments:
	echo "test-nocomments:"
	$(PGN_EXTRACT) -C -otest-CC-out.pgn --quiet $(INPUT)$(SEP)test-C.pgn
	$(CMP) test-CC-out.pgn $(OUTPUT)$(SEP)test-CC-out.pgn

# -d $(SEP) --duplicates
#     + Input file containing games with duplicates and non-duplicates.
#     - Input file(s): fischer.pgn, $(INPUT)$(SEP)petrosian.pgn
#     - Resulting output should be files separating the unique and
#       duplicated games in the input files.
#     - Expected output: test-d-unique.pgn, test-d-dupes.pgn
test-duplicates:
	echo "test-duplicates:"
	$(PGN_EXTRACT) -C -dtest-d-dupes.pgn -otest-d-unique.pgn --quiet $(INPUT)$(SEP)fischer.pgn $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-d-dupes.pgn $(OUTPUT)$(SEP)test-d-dupes.pgn
	$(CMP) test-d-unique.pgn $(OUTPUT)$(SEP)test-d-unique.pgn

# -D $(SEP) --noduplicates
#     + Input file containing games with duplicates and non-duplicates.
#     - Input file(s): fischer.pgn, $(INPUT)$(SEP)petrosian.pgn
#     - Resulting output should be a file containing the combined input
#       with duplicate games removed.
#     - Expected output: test-DD-unique.pgn
test-noduplicates:
	echo "test-noduplicates:"
	$(PGN_EXTRACT) -D -otest-DD-unique.pgn --quiet $(INPUT)$(SEP)fischer.pgn $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-DD-unique.pgn $(OUTPUT)$(SEP)test-DD-unique.pgn
	$(PGN_EXTRACT) --noduplicates -otest-DD-unique.pgn --quiet $(INPUT)$(SEP)fischer.pgn $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-DD-unique.pgn $(OUTPUT)$(SEP)test-DD-unique.pgn

# -e
#     + Input file containing games without ECO classifications.
#     - Input file(s): test-e.pgn and eco.pgn in the test folder.
#     - Resulting output should have ECO classification added to the tags.
#     - Expected output: test-e-out.pgn
test-e:
	echo "test-e:"
	$(PGN_EXTRACT) -e$(ECO_FILE) -otest-e-out.pgn --quiet $(INPUT)$(SEP)test-e.pgn
	$(CMP) test-e-out.pgn $(OUTPUT)$(SEP)test-e-out.pgn

# -E
#     + Input file containing games.
#     - Input file(s): test-ucE.pgn and eco.pgn if -e flag is used.
#     - Resulting output should be separate files for each ECO classification
#       at the level of the initial letter (A-E).
#	NB: The test removes existing A-E files first because this option appends
#	to existing files, which breaks the comparison test otherwise.
#     - Expected output: A.pgn, B.pgn, E.pgn
test-EE:
	echo "test-EE:"
	-$(RM) [A-E].pgn
	$(PGN_EXTRACT) -e$(ECO_FILE) -E1 --quiet $(INPUT)$(SEP)test-ucE.pgn
	$(CMP) A.pgn $(OUTPUT)$(SEP)A.pgn
	$(CMP) B.pgn $(OUTPUT)$(SEP)B.pgn
	$(CMP) E.pgn $(OUTPUT)$(SEP)E.pgn

# -f
#     + Input files containing games.
#     - Input file(s): test-f1.pgn, test-f2.pgn, files.txt
#     - Resulting output should be the combination of the input files.
#     - Expected output: test-f-out.pgn
test-f:
	echo "test-f:"
	$(PGN_EXTRACT) --quiet -f$(INPUT)$(SEP)files.txt -otest-f-out.pgn
	$(CMP) test-f-out.pgn $(OUTPUT)$(SEP)test-f-out.pgn

# -F:
#     + Input file containing games without a trailing FEN comment.
#     - Input file(s): test-F.pgn
#     - Resulting output should have a comment at the end containing
#       the FEN description of the final position.
#     - Expected output: test-FF-out.pgn
test-FF:
	echo "test-FF:"
	$(PGN_EXTRACT) -F -otest-FF-out.pgn --quiet $(INPUT)$(SEP)test-F.pgn
	$(CMP) test-FF-out.pgn $(OUTPUT)$(SEP)test-FF-out.pgn
	$(PGN_EXTRACT) -Fdiagram -otest-FF-text-out.pgn --quiet $(INPUT)$(SEP)test-F-text.pgn
	$(CMP) test-FF-text-out.pgn $(OUTPUT)$(SEP)test-FF-text-out.pgn

# 
# -h $(SEP) -? $(SEP) --help
#     + No input required.
#     - Input file(s): N$(SEP)A
#     - Resulting output should be a description of how to use pgn-extract
#     - Expected output: N$(SEP)A
# Also:
# 	..$(SEP)pgn-extract -?
# 	..$(SEP)pgn-extract --help
test-h:
	echo "test-h:"
	-$(PGN_EXTRACT) -h

# -l
#     + Input file containing games.
#     - Input file(s): fischer.pgn
#     - Resulting output: List of games parsed written to log.txt
#     - Expected output: log.txt
test-l:
	echo "test-l:"
	$(PGN_EXTRACT) -llog.txt -otest-l-out.pgn $(INPUT)$(SEP)fischer.pgn
	$(CMP) log.txt $(OUTPUT)$(SEP)log.txt
	$(CMP) test-l-out.pgn $(OUTPUT)$(SEP)test-l-out.pgn

# -L
#     + Input file containing games.
#     - Input file(s): test-L1.pgn, test-L2.pgn
#     - Resulting output should be that log.txt contains the combined
#       logs from two runs of pgn-extract.
#     - Expected output: log.txt
test-LL:
	echo "test-LL:"
	$(PGN_EXTRACT) -lLLog.txt -r $(INPUT)$(SEP)test-L1.pgn
	$(PGN_EXTRACT) -LLLog.txt -r $(INPUT)$(SEP)test-L2.pgn
	$(CMP) LLog.txt $(OUTPUT)$(SEP)LLog.txt

# -M $(SEP) --checkmate
#     + Input file containing games.
#     - Input file(s): test-checkmate.pgn
#     - Resulting output should contain only those games that end in checkmate.
#     - Expected output: test-checkmate-out.pgn
test-checkmate:
	echo "test-checkmate:"
	$(PGN_EXTRACT) --checkmate -otest-checkmate-out.pgn --quiet $(INPUT)$(SEP)test-checkmate.pgn
	$(CMP) test-checkmate-out.pgn $(OUTPUT)$(SEP)test-checkmate-out.pgn

# -n
#     + Input file containing games.
#     - Input file(s): petrosian.pgn
#     - Resulting output should be separate files containing matched
#       and non-matched games.
#     - Expected output: test-n-matched.pgn, test-n-unmatched.pgn
test-n:
	echo "test-n:"
	$(PGN_EXTRACT) -TpFischer -otest-n-matched.pgn -ntest-n-unmatched.pgn --quiet $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-n-matched.pgn $(OUTPUT)$(SEP)test-n-matched.pgn
	$(CMP) test-n-unmatched.pgn $(OUTPUT)$(SEP)test-n-unmatched.pgn

# --suppressmatched
#     + Input file containing games.
#     - Input file(s): fischer.pgn
#     - Resulting output should be a file of non-Petrosian games
#     - Expected output: non-petrosian.pgn
test-suppressmatched:
	echo "test-suppressmatched:"
	$(PGN_EXTRACT) -TpPetrosian --suppressmatched -nnon-petrosian.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) non-petrosian.pgn $(OUTPUT)$(SEP)non-petrosian.pgn

# -N $(SEP) --nonags
#     + Input file containing games with NAGs.
#     - Input file(s): test-N.pgn
#     - Resulting output should have all NAGs removed.
#     - Expected output: test-NN-out.pgn
test-NN:
	echo "test-NN:"
	$(PGN_EXTRACT) -N -otest-NN-out.pgn --quiet $(INPUT)$(SEP)test-N.pgn
	$(CMP) test-NN-out.pgn $(OUTPUT)$(SEP)test-NN-out.pgn
#       Comments after multiple nags.
	$(PGN_EXTRACT) -otest-nagcomments-out.pgn --quiet $(INPUT)$(SEP)test-nagcomments.pgn
#       Deleting NAGs with comments after multiple nags.
	$(PGN_EXTRACT) -N -atest-nagcomments-out.pgn --quiet $(INPUT)$(SEP)test-nagcomments.pgn
	$(CMP) test-nagcomments-out.pgn $(OUTPUT)$(SEP)test-nagcomments-out.pgn
	$(CMP) test-nagcomments-out.pgn $(OUTPUT)$(SEP)test-nagcomments-out.pgn

# 
# -o $(SEP) --output
#     + Input file containing games in any accepted format.
#     - Input file(s): test-o.txt
#     - Resulting output should contain the input game formatted in PGN.
#     - Expected output: test-o-out.pgn
test-output:
	echo "test-output:"
	$(PGN_EXTRACT) -otest-o-out.pgn --quiet $(INPUT)$(SEP)test-o.txt
	$(CMP) test-o-out.pgn $(OUTPUT)$(SEP)test-o-out.pgn
	$(PGN_EXTRACT) --output test-o-out.pgn --quiet $(INPUT)$(SEP)test-o.txt
	$(CMP) test-o-out.pgn $(OUTPUT)$(SEP)test-o-out.pgn

# -P
#     + Input file containing games with different sequences for the same
#       opening.
#     - Input file(s): test-P.pgn, Pvars.txt
#     - Resulting output should be games whose opening move exactly match
#       the sequence specified in Pvars.txt
#     - Expected output: test-PP-out.pgn
test-PP:
	echo "test-PP:"
	$(PGN_EXTRACT) -P -v$(INPUT)$(SEP)Pvars.txt -otest-PP-out.pgn --quiet $(INPUT)$(SEP)test-P.pgn
	$(CMP) test-PP-out.pgn $(OUTPUT)$(SEP)test-PP-out.pgn

# -r
#     + Input file containing games in any accepted format.
#     - Input file(s): test-r.text
#     - Resulting output should contain tag summary lines for the games
#       matched and a report of any errors.
#     - Expected output: test-r-log.txt
test-r:
	echo "test-r:"
	$(PGN_EXTRACT) -r -ltest-r-log.txt $(INPUT)$(SEP)test-r.txt
#       $(CMP) test-r-log.txt $(OUTPUT)$(SEP)test-r-log.txt

# -R
#     + Input file containing games.
#     - Input file(s): test-R.pgn, roster.txt
#     - Resulting output should contain games with their tag roster in
#       the order specified in roster.txt
#     - Expected output: test-R-out.pgn
test-RR:
	echo "test-RR:"
	$(PGN_EXTRACT) -R$(INPUT)$(SEP)roster.txt --output test-RR-out.pgn --quiet $(INPUT)$(SEP)test-R.pgn
	$(CMP) test-RR-out.pgn $(OUTPUT)$(SEP)test-RR-out.pgn

# -s
#     + Input file containing games.
#     - Input file(s): test-s.pgn
#     - Resulting output should be silent, with games written to the output file.
#     - Expected output: test-s-out.pgn
test-s:
	echo "test-s:"
	$(PGN_EXTRACT) -s -o test-s-out.pgn $(INPUT)$(SEP)test-s.pgn
	$(CMP) test-s-out.pgn $(OUTPUT)$(SEP)test-s-out.pgn

# -S
#     + Input file containing games whose players' names have slight
#       sound variations from anglesized versions.
#     - Input file(s): test-ucS.pgn
#     - Resulting output should be games that match by ignoring slight
#       soundex differences.
#     - Expected output: test-SS-out.pgn
test-SS:
	echo "test-SS:"
	$(PGN_EXTRACT) -S -TpPetrosian -otest-SS-out.pgn --quiet $(INPUT)$(SEP)test-ucS.pgn
	$(CMP) test-SS-out.pgn $(OUTPUT)$(SEP)test-SS-out.pgn

# -t
#     + Input file containing games and a file of tag criteria.
#     - Input file(s): test-t.pgn, taglist.txt
#     - Resulting output should be only those games whose tags match
#       all of the criteria.
#     - Expected output: test-t-out.pgn
test-t:
	echo "test-t:"
	$(PGN_EXTRACT) -t$(INPUT)$(SEP)taglist.txt -otest-t-out.pgn --quiet $(INPUT)$(SEP)test-t.pgn
	$(CMP) test-t-out.pgn $(OUTPUT)$(SEP)test-t-out.pgn
	$(PGN_EXTRACT) -t$(INPUT)$(SEP)test-tFEN.txt -otest-tFEN-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-tFEN-out.pgn $(OUTPUT)$(SEP)test-tFEN-out.pgn

# -T
#     + Input file containing games with tag information.
#     - Input file(s): fischer.pgn, test-Ta.pgn (and eco.pgn for -Te test.)
#     - Resulting output should contain only those games whose tag information
#       matches that specified.
#     - Expected output: test-Ta-out.pgn, test-Tb-out.pgn, test-Td-out.pgn,
#                        test-Tdd-out.pgn test-Te-out.pgn, test-Tp-out.pgn,
#                        test-Tw-out.pgn
test-TT:
	echo "test-TT:"
	$(PGN_EXTRACT) -Td1970 -otest-TTd-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-TTd-out.pgn $(OUTPUT)$(SEP)test-TTd-out.pgn
	$(PGN_EXTRACT) -Td1960 -Td1962 -otest-TTdd-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-TTdd-out.pgn $(OUTPUT)$(SEP)test-TTdd-out.pgn
	$(PGN_EXTRACT) -TbPetrosian -otest-TTb-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-TTb-out.pgn $(OUTPUT)$(SEP)test-TTb-out.pgn
	$(PGN_EXTRACT) -e$(ECO_FILE) -TeB14 -otest-Te-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(PGN_EXTRACT) -TpPetrosian -otest-TTp-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-TTp-out.pgn $(OUTPUT)$(SEP)test-TTp-out.pgn
	$(PGN_EXTRACT) -Tr0-1 -otest-TTr-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-TTr-out.pgn $(OUTPUT)$(SEP)test-TTr-out.pgn
	$(PGN_EXTRACT) -TwFischer -otest-TTw-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-TTw-out.pgn $(OUTPUT)$(SEP)test-TTw-out.pgn
	$(PGN_EXTRACT) -TaBarnes -otest-TTa-out.pgn --quiet $(INPUT)$(SEP)test-Ta.pgn
	$(CMP) test-TTa-out.pgn $(OUTPUT)$(SEP)test-TTa-out.pgn

# -U $(SEP) --nounique
#     + Input file containing games with duplicates and non-duplicates.
#     - Input file(s): fischer.pgn, petrosian.pgn
#     - Resulting output should be a file containing just the duplicate games.
#     - Expected output: test-U-unique.pgn
test-nounique:
	echo "test-nounique:"
	$(PGN_EXTRACT) -U -otest-UU-unique.pgn --quiet $(INPUT)$(SEP)fischer.pgn $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-UU-unique.pgn $(OUTPUT)$(SEP)test-UU-unique.pgn
	$(PGN_EXTRACT) --nounique -otest-UU-unique.pgn --quiet $(INPUT)$(SEP)fischer.pgn $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-UU-unique.pgn $(OUTPUT)$(SEP)test-UU-unique.pgn

# -v
#     + Input file containing games.
#     - Input file(s): najdorf.pgn, vvars.txt
#     - Resulting output should be only those games whose opening moves
#       textually match (in any order) the moves in vars.txt.
#     - Expected output: test-v-out.pgn
test-v:
	echo "test-v:"
	$(PGN_EXTRACT) -v$(INPUT)$(SEP)vvars.txt -otest-v-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(CMP) test-v-out.pgn $(OUTPUT)$(SEP)test-v-out.pgn

# -V
#     + Input file containing games with variations
#     - Input file(s): test-V.pgn
#     - Resulting output should have all variations removed.
#     - Expected output: test-V-out.pgn
test-VV:
	echo "test-VV:"
	$(PGN_EXTRACT) -V -otest-VV-out.pgn --quiet $(INPUT)$(SEP)test-V.pgn
	$(CMP) test-VV-out.pgn $(OUTPUT)$(SEP)test-VV-out.pgn

# -w $(SEP) --linelength
#     + Input file containing games.
#     - Input file(s): test-w.pgn
#     - Resulting output Games formatted up to the specified line length.
#       The default is 75.
#     - Expected output: test-w60-out.pgn, test-w75-out.pgn, test-w1000-out.pgn
test-linelength:
	echo "test-linelength:"
	$(PGN_EXTRACT) -w60 -otest-w60-out.pgn --quiet $(INPUT)$(SEP)test-w.pgn
	$(CMP) test-w60-out.pgn $(OUTPUT)$(SEP)test-w60-out.pgn
	$(PGN_EXTRACT) -w75 -otest-w75-out.pgn --quiet $(INPUT)$(SEP)test-w.pgn
	$(CMP) test-w75-out.pgn $(OUTPUT)$(SEP)test-w75-out.pgn
	$(PGN_EXTRACT) -w1000 -otest-w1000-out.pgn --quiet $(INPUT)$(SEP)test-w.pgn
	$(CMP) test-w1000-out.pgn $(OUTPUT)$(SEP)test-w1000-out.pgn

	$(PGN_EXTRACT) --linelength 60 -otest-w60-out.pgn --quiet $(INPUT)$(SEP)test-w.pgn
	$(CMP) test-w60-out.pgn $(OUTPUT)$(SEP)test-w60-out.pgn
	$(PGN_EXTRACT) --linelength 75 -otest-w75-out.pgn --quiet $(INPUT)$(SEP)test-w.pgn
	$(CMP) test-w75-out.pgn $(OUTPUT)$(SEP)test-w75-out.pgn
	$(PGN_EXTRACT) --linelength 1000 -otest-w1000-out.pgn --quiet $(INPUT)$(SEP)test-w.pgn
	$(CMP) test-w1000-out.pgn $(OUTPUT)$(SEP)test-w1000-out.pgn

# -W
#     + Input file containing games.
#     - Input file(s): test-ucW.pgn
#     - Resulting output should be games formatted in the specified notation:
#       halg (hyphenated long algebraic), lalg (non-hyphenated long algebraic),
#       elalg (enhanced long algebraic), xlalg (enhanced with capture info),
#       uci (UCI-compatible output),
#	and alternative piece letters.
#     - Expected output: test-WWhalg-out.pgn, test-WWlalg-out.pgn,
#                        test-WWelalg-out.pgn, test-WWdeutsch-out.pgn,
#                        test-WWuci-out.pgn
test-WW:
	echo "test-WW:"
	$(PGN_EXTRACT) -Whalg -otest-WWhalg-out.pgn --quiet $(INPUT)$(SEP)test-ucW.pgn
	$(CMP) test-WWhalg-out.pgn $(OUTPUT)$(SEP)test-WWhalg-out.pgn
	$(PGN_EXTRACT) -Wlalg -otest-WWlalg-out.pgn --quiet $(INPUT)$(SEP)test-ucW.pgn
	$(CMP) test-WWlalg-out.pgn $(OUTPUT)$(SEP)test-WWlalg-out.pgn
	$(PGN_EXTRACT) -Welalg -otest-WWelalg-out.pgn --quiet $(INPUT)$(SEP)test-ucW.pgn
	$(CMP) test-WWelalg-out.pgn $(OUTPUT)$(SEP)test-WWelalg-out.pgn
	$(PGN_EXTRACT) -Wxlalg -otest-WWxlalg-out.pgn --quiet $(INPUT)$(SEP)test-ucW.pgn
	$(CMP) test-WWxlalg-out.pgn $(OUTPUT)$(SEP)test-WWxlalg-out.pgn
	$(PGN_EXTRACT) -WsanBSLTDK -otest-WWdeutsch-out.pgn --quiet $(INPUT)$(SEP)test-ucW.pgn
	$(CMP) test-WWdeutsch-out.pgn $(OUTPUT)$(SEP)test-WWdeutsch-out.pgn
	$(PGN_EXTRACT) -Wuci -otest-WWuci-out.pgn --quiet $(INPUT)$(SEP)test-ucW.pgn
	$(CMP) test-WWuci-out.pgn $(OUTPUT)$(SEP)test-WWuci-out.pgn
	$(PGN_EXTRACT) -Wepd -otest-WWepd-out.pgn --quiet $(INPUT)$(SEP)test-ucW.pgn
	$(CMP) test-WWepd-out.pgn $(OUTPUT)$(SEP)test-WWepd-out.pgn

# -x
#     + Input file containing games.
#     - Input file(s): najdorf.pgn, xvars.txt
#     - Resulting output should be only those games which match
#       the result of reaching the opening sequence in vars.txt.
#     - Expected output: test-x-out.pgn
test-x:
	echo "test-x:"
	$(PGN_EXTRACT) -x$(INPUT)$(SEP)xvars.txt -otest-x-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(CMP) test-x-out.pgn $(OUTPUT)$(SEP)test-x-out.pgn
	$(PGN_EXTRACT) -x$(INPUT)$(SEP)xvars.txt -otest-x-out.pgn -ntest-xn-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(CMP) test-xn-out.pgn $(OUTPUT)$(SEP)test-xn-out.pgn

# -y
#     + Input file containing games.
#     - Input file(s): fischer.pgn, zmatch.txt
#     - Resulting output should be games whose material balance matches that
#       specified in ymatch.txt
#     - Expected output: test-y-out.pgn
test-y:
	echo "test-y:"
	$(PGN_EXTRACT) -y$(INPUT)$(SEP)ymatch.txt --markmatches MATCH -otest-y-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-y-out.pgn $(OUTPUT)$(SEP)test-y-out.pgn
	$(PGN_EXTRACT) --materialy "q*r*p*b2n2< q=r=p=b2<n2" --markmatches MATCH -otest-y-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-y-out.pgn $(OUTPUT)$(SEP)test-y-out.pgn

# -z
#     + Input file containing games.
#     - Input file(s): fischer.pgn, zmatch.txt
#     - Resulting output should be games whose material balance matches that
#       specified in zmatch.txt
#     - Expected output: test-z-out.pgn
test-z:
	echo "test-z:"
	$(PGN_EXTRACT) -z$(INPUT)$(SEP)zmatch.txt -otest-z-out.pgn --markmatches MATCH --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-z-out.pgn $(OUTPUT)$(SEP)test-z-out.pgn
	$(PGN_EXTRACT) --materialz "q*r*p*b2n2< q=r=p=b2<n2" -otest-z-out.pgn --markmatches MATCH --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-z-out.pgn $(OUTPUT)$(SEP)test-z-out.pgn

# -Z
#     + Input file containing games with duplicates and non-duplicates.
#     - Input file(s): fischer.pgn, petrosian.pgn
#     - Resulting output should be files separating the unique and
#       duplicated games in the input files.
#     - Expected output: test-ZZ-unique.pgn, test-ZZ-dupes.pgn
test-ZZ:
	echo "test-ZZ:"
	$(PGN_EXTRACT) -C -Z -dtest-ZZ-dupes.pgn -otest-ZZ-unique.pgn --quiet $(INPUT)$(SEP)fischer.pgn $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-ZZ-dupes.pgn $(OUTPUT)$(SEP)test-ZZ-dupes.pgn
	$(CMP) test-ZZ-unique.pgn $(OUTPUT)$(SEP)test-ZZ-unique.pgn

# -#
#     + Input file containing games.
#     - Input file(s): test-hash.pgn
#     - Resulting output The input file split in to separate sub-files,
#       each containing 10 games, except the last which may contain fewer.
#     - Expected output: 1.pgn, 2.pgn
test-hash:
	echo "test-hash:"
	$(PGN_EXTRACT) -#20 --quiet $(INPUT)$(SEP)test-hash.pgn
	$(CMP) 1.pgn $(OUTPUT)$(SEP)1.pgn
	$(CMP) 2.pgn $(OUTPUT)$(SEP)2.pgn

	$(PGN_EXTRACT) -#20,10 --quiet $(INPUT)$(SEP)test-hash.pgn
	$(CMP) 10.pgn $(OUTPUT)$(SEP)10.pgn
	$(CMP) 11.pgn $(OUTPUT)$(SEP)11.pgn

# --evaluation
#     + Input file containing games.
#     - Input file(s): test-evaluation.pgn
#     - Resulting output should include an evaluation value in a comment
#       after every move.
#     - Expected output: test-evaluation-out.pgn
test-evaluation:
	echo "test-evaluation:"
	$(PGN_EXTRACT) --evaluation -otest-evaluation-out.pgn --quiet $(INPUT)$(SEP)test-evaluation.pgn
	$(CMP) test-evaluation-out.pgn $(OUTPUT)$(SEP)test-evaluation-out.pgn

# --fencomments
#     + Input file containing games.
#     - Input file(s): test-fencomments.pgn
#     - Resulting output should have a comment after every move containing a
#       FEN description of the position after that move.
#     - Expected output: test-fencomments-out.pgn
test-fencomments:
	echo "test-fencomments:"
	$(PGN_EXTRACT) --fencomments -otest-fencomments-out.pgn --quiet $(INPUT)$(SEP)test-fencomments.pgn
	$(CMP) test-fencomments-out.pgn $(OUTPUT)$(SEP)test-fencomments-out.pgn

# --markmatches
#     + Input file containing games.
#     - Input file(s): najdorf.pgn, xvars.txt
#     - Resulting output should be only those games which match
#       the result of reaching the opening sequence in vars.txt.
#       The point of each match is marked with the comment { MATCH }
#     - Expected output: test-markmatches-out.pgn
test-markmatches:
	echo "test-markmatches:"
	$(PGN_EXTRACT) --markmatches MATCH -x$(INPUT)$(SEP)xvars.txt -otest-markmatches-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(CMP) test-markmatches-out.pgn $(OUTPUT)$(SEP)test-markmatches-out.pgn

# --nestedcomments
#     + Input file containing games.
#     - Input file(s): nested-comment.pgn
#     - Resulting output should have retained the nested comment.
#     - Expected output: test-nestedcomments-out.pgn
test-nestedcomments:
	echo "test-nestedcomments:"
	$(PGN_EXTRACT) --nestedcomments -otest-nestedcomments-out.pgn --quiet $(INPUT)$(SEP)nested-comment.pgn
	$(CMP) test-nestedcomments-out.pgn $(OUTPUT)$(SEP)test-nestedcomments-out.pgn

# --nochecks
#     + Input file containing games with moves involving moves that give check
#       and$(SEP)or mate.
#     - Input file(s): test-nochecks.pgn
#     - Resulting output should contain games with no check indicators after moves.
#     - Expected output: test-nochecks-out.pgn
test-nochecks:
	echo "test-nochecks:"
	$(PGN_EXTRACT) --nochecks -otest-nochecks-out.pgn --quiet $(INPUT)$(SEP)test-nochecks.pgn
	$(CMP) test-nochecks-out.pgn $(OUTPUT)$(SEP)test-nochecks-out.pgn

# --nomovenumbers
#     + Input file containing games with move numbers.
#     - Input file(s): test-nomovenumbers.pgn
#     - Resulting output should contain games with no move numbers.
#     - Expected output: test-nomovenumbers-out.pgn
test-nomovenumbers:
	echo "test-nomovenumbers:"
	$(PGN_EXTRACT) -otest-nomovenumbers-out.pgn --nomovenumbers --quiet $(INPUT)$(SEP)test-nomovenumbers.pgn
	$(CMP) test-nomovenumbers-out.pgn $(OUTPUT)$(SEP)test-nomovenumbers-out.pgn

# --noresults
#     + Input file containing games with results.
#     - Input file(s): test-noresults.pgn
#     - Resulting output should contain games with no results.
#     - Expected output: test-noresults-out.pgn
test-noresults:
	echo "test-noresults:"
	$(PGN_EXTRACT) -otest-noresults-out.pgn --noresults --quiet $(INPUT)$(SEP)test-noresults.pgn
	$(CMP) test-noresults-out.pgn $(OUTPUT)$(SEP)test-noresults-out.pgn

# --notags
#     + Input file containing games with tag information.
#     - Input file(s): test-notags.pgn
#     - Resulting output should contain games with no tag information.
#     - Expected output: test-notags-out.pgn
test-notags:
	echo "test-notags:"
	$(PGN_EXTRACT) -otest-notags-out.pgn --notags --quiet $(INPUT)$(SEP)test-notags.pgn
	$(CMP) test-notags-out.pgn $(OUTPUT)$(SEP)test-notags-out.pgn

# --plylimit
#     + Input file containing games.
#     - Input file(s): test-plylimit.pgn
#     - Resulting output should contain games whose number of moves (plies) are
#       limited at the specified ply limit.
#     - Expected output: test-plylimit-out.pgn
test-plylimit:
	echo "test-plylimit:"
	$(PGN_EXTRACT) --plylimit 10 -otest-plylimit-out.pgn --quiet $(INPUT)$(SEP)test-plylimit.pgn
	$(CMP) test-plylimit-out.pgn $(OUTPUT)$(SEP)test-plylimit-out.pgn

# --dropply
#     + Input file containing games.
#     - Input file(s): najdorf.pgn
#     - Resulting output should contain games whose number of moves (plies) are
#       have been reduced by the given number from the beginning or end.
#     - Expected output: test-dropply-out.pgn
test-dropply:
	echo "test-dropply:"
	$(PGN_EXTRACT) --dropply 10 -otest-dropply-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(PGN_EXTRACT) --dropply 11 -atest-dropply-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(PGN_EXTRACT) --dropply -1 -atest-dropply-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(PGN_EXTRACT) --dropply -2 -atest-dropply-out.pgn --quiet $(INPUT)$(SEP)najdorf.pgn
	$(CMP) test-dropply-out.pgn $(OUTPUT)$(SEP)test-dropply-out.pgn


# -t
#     + Input file containing games and a file of tag criteria.
#     - Input file(s): test-bots.pgn, reltags.txt
#     - Resulting output should be only those games whose tags match
#       all of the criteria. The matches involve relational operators.
#     - Expected output: test-reltags-out.pgn
test-reltags:
	echo "test-reltags:"
	$(PGN_EXTRACT) -t$(INPUT)$(SEP)reltags.txt -otest-reltags-out.pgn --quiet $(INPUT)$(SEP)bots.pgn
	$(CMP) test-reltags-out.pgn $(OUTPUT)$(SEP)test-reltags-out.pgn

# --selectonly
#     + Input file containing games.
#     - Input file(s): test-selectonly.pgn
#     - Resulting output should contain only two matched games.
#     - Expected output: test-selectonly-out.pgn
test-selectonly:
	echo "test-selectonly:"
	$(PGN_EXTRACT) -TpPetrosian --selectonly 2,3 -otest-selectonly-out.pgn --quiet $(INPUT)$(SEP)test-selectonly.pgn
	$(CMP) test-selectonly-out.pgn $(OUTPUT)$(SEP)test-selectonly-out.pgn

# --skipmatching
#     + Input file containing games.
#     - Input file(s): test-skipmatching.pgn
#     - Resulting output should contain a single matched game.
#     - Expected output: test-selectonly-out.pgn
test-skipmatching:
	echo "test-skipmatching:"
	$(PGN_EXTRACT) -TpKeres --skipmatching 1,2 -otest-skipmatching-out.pgn --quiet $(INPUT)$(SEP)fischer.pgn
	$(CMP) test-skipmatching-out.pgn $(OUTPUT)$(SEP)test-skipmatching-out.pgn

# --splitvariants
#     + Input file containing games.
#     - Input file(s): test-splitvariants.pgn
#     - Resulting output should contain a separate game
#       for every variation in the input file.
#     - Expected output: test-splitvariants-out.pgn
test-splitvariants:
	echo "test-splitvariants:"
	$(PGN_EXTRACT) --splitvariants -otest-splitvariants-out.pgn --quiet $(INPUT)$(SEP)test-splitvariants.pgn
	$(CMP) test-splitvariants-out.pgn $(OUTPUT)$(SEP)test-splitvariants-out.pgn

# --stalemate
#     + Input file containing games.
#     - Input file(s): test-stalemate.pgn
#     - Resulting output should contain only those games that end in stalemate.
#     - Expected output: test-stalemate-out.pgn
test-stalemate:
	echo "test-stalemate:"
	$(PGN_EXTRACT) --stalemate -otest-stalemate-out.pgn --quiet $(INPUT)$(SEP)test-stalemate.pgn
	$(CMP) test-stalemate-out.pgn $(OUTPUT)$(SEP)test-stalemate-out.pgn

# --fifty
#     + Input file containing games.
#     - Input file(s): test-fifty.pgn
#     - Resulting output should contain only those games to which the fifty-move rule
#       applies, with the comment { FIFTY } at the approriate point.
#     - Expected output: test-fifty-out.pgn
test-fifty:
	echo "test-fifty:"
	$(PGN_EXTRACT) --fifty --markmatches FIFTY -otest-fifty-out.pgn --quiet $(INPUT)$(SEP)test-fifty.pgn
	$(CMP) test-fifty-out.pgn $(OUTPUT)$(SEP)test-fifty-out.pgn

# --seventyfive
#     + Input file containing games.
#     - Input file(s): test-seventyfive.pgn
#     - Resulting output should contain only those games to which the seventy-five-move rule
#       applies, with the comment { SEVENTYFIVE } at the approriate point.
#     - Expected output: test-seventyfive-out.pgn
test-seventyfive:
	echo "test-seventyfive:"
	$(PGN_EXTRACT) --seventyfive --markmatches SEVENTYFIVE -otest-seventyfive-out.pgn --quiet $(INPUT)$(SEP)test-seventyfive.pgn
	$(CMP) test-seventyfive-out.pgn $(OUTPUT)$(SEP)test-seventyfive-out.pgn

# -t tags with EloDiff match
#     + Input file containing games.
#     - Input file(s): test-seventyfive.pgn
#     - Resulting output should contain only those games with a limited Elo difference.
#     - Expected output: test-elodiff-out.pgn
test-elodiff:
	echo "test-elodiff:"
	$(PGN_EXTRACT) -t $(INPUT)/test-elodiff.txt -otest-elodiff-out.pgn --quiet $(INPUT)$(SEP)test-seventyfive.pgn
	$(CMP) test-elodiff-out.pgn $(OUTPUT)$(SEP)test-elodiff-out.pgn

# --repetition
#     + Input file containing games.
#     - Input file(s): test-repetition.pgn
#     - Resulting output should contain only those games which contain a 3-fold repetition,
#       with the comment { REPEAT } at the approriate point.
#     - Expected output: test-repetition-out.pgn
test-repetition:
	echo "test-repetition:"
	$(PGN_EXTRACT) --repetition --markmatches REPEAT -otest-repetition-out.pgn --quiet $(INPUT)$(SEP)test-repetition.pgn
	$(CMP) test-repetition-out.pgn $(OUTPUT)$(SEP)test-repetition-out.pgn

# --plycount and --totalplycount
#     + Input file containing games.
#     - Input file(s): test-plycount.pgn
#     - Resulting output should contain games with PlyCount and TotalPlyCount tags.
#     - Expected output: test-plycount-out.pgn
test-plycount:
	echo "test-plycount:"
	$(PGN_EXTRACT) --plycount --totalplycount -otest-plycount-out.pgn --quiet $(INPUT)$(SEP)test-plycount.pgn
	$(CMP) test-plycount-out.pgn $(OUTPUT)$(SEP)test-plycount-out.pgn

# --addhashcode
#     + Input file containing games.
#     - Input file(s): test-addhashcode.pgn
#     - Resulting output should contain games with a HashCode tag.
#     - Expected output: test-addhashcode-out.pgn
test-addhashcode:
	echo "test-addhashcode:"
	$(PGN_EXTRACT) --addhashcode -otest-addhashcode-out.pgn --quiet $(INPUT)$(SEP)test-addhashcode.pgn
	$(CMP) test-addhashcode-out.pgn $(OUTPUT)$(SEP)test-addhashcode-out.pgn

# Test on a file with a string too long to be output within the
# defined line length.
#     + Input file containing a game with a very long comment.
#     + Input file(s): test-long-line.pgn
#     + Resulting output should contain the game properly formatted and
#       the log file should contain an error message reporting the problem.
#     + Expected output: test-long-line-out.pgn, test-long-line-log.txt
test-long-line:
	echo "test long line:"
	$(PGN_EXTRACT) -otest-long-line-out.pgn -ltest-long-line-log.txt $(INPUT)$(SEP)test-long-line.pgn
	$(CMP) test-long-line-out.pgn $(OUTPUT)$(SEP)test-long-line-out.pgn
#       $(CMP) test-long-line-log.txt $(OUTPUT)$(SEP)test-long-line-log.txt

# Promotion characters.
#     + Input file with games involving a promotion
#     - Input file(s): test-promotion-in.pgn
#     - Game output should have all games with promotions.
#     - Expected output: test-promotion-out.pgn
test-promotion:
	echo "test-promotion:"
	$(PGN_EXTRACT) -otest-promotion-out.pgn --quiet $(INPUT)$(SEP)test-promotion-in.pgn
	$(CMP) test-promotion-out.pgn $(OUTPUT)$(SEP)test-promotion-out.pgn
	$(PGN_EXTRACT) --underpromotion -otest-underpromotion-out.pgn --quiet $(INPUT)$(SEP)test-promotion-in.pgn
	$(CMP) test-underpromotion-out.pgn $(OUTPUT)$(SEP)test-underpromotion-out.pgn

# Inconsistent Result tags
#     + Input file with games where the Result tag conflicts with checkmate or stalemate.
#     - Input file(s): test-fixresulttags-in.pgn
#     - Game output should have consistent Result tags
#     - Expected output: test-fixresulttags-out.pgn
test-fixresulttags:
	echo "test-fixresulttags:"
	$(PGN_EXTRACT) --fixresulttags -otest-fixresulttags-out.pgn -lfix-results-log.txt --quiet $(INPUT)$(SEP)test-fixresulttags-in.pgn
	$(CMP) test-fixresulttags-out.pgn $(OUTPUT)$(SEP)test-fixresulttags-out.pgn
#       $(CMP) fix-results-log.txt $(OUTPUT)$(SEP)fix-results-log.txt

# --fuzzydepth
#     + Input file containing games with fuzzy duplicates and non-duplicates.
#     - Input file(s): $(INPUT)$(SEP)test-fuzzydepth.pgn
#     - Resulting output should be files separating the unique and
#       duplicated games in the input files.
#     - Expected output: test-fuzzydepth0-unique.pgn, test-fuzzydepth0-dupes.pgn
test-fuzzydepth:
	echo "test-duplicates:"
	$(PGN_EXTRACT) -C --fuzzydepth 0 -dtest-fuzzydepth0-dupes.pgn -otest-fuzzydepth0-unique.pgn --quiet $(INPUT)$(SEP)test-fuzzydepth.pgn
	$(CMP) test-fuzzydepth0-dupes.pgn $(OUTPUT)$(SEP)test-fuzzydepth0-dupes.pgn
	$(CMP) test-fuzzydepth0-unique.pgn $(OUTPUT)$(SEP)test-fuzzydepth0-unique.pgn
	$(PGN_EXTRACT) -C --fuzzydepth 3 -dtest-fuzzydepth3-dupes.pgn -otest-fuzzydepth3-unique.pgn --quiet $(INPUT)$(SEP)test-fuzzydepth.pgn
	$(CMP) test-fuzzydepth3-dupes.pgn $(OUTPUT)$(SEP)test-fuzzydepth3-dupes.pgn
	$(CMP) test-fuzzydepth3-unique.pgn $(OUTPUT)$(SEP)test-fuzzydepth3-unique.pgn
	$(PGN_EXTRACT) -C --fuzzydepth 4 -dtest-fuzzydepth4-dupes.pgn -otest-fuzzydepth4-unique.pgn --quiet $(INPUT)$(SEP)test-fuzzydepth.pgn
	$(CMP) test-fuzzydepth4-dupes.pgn $(OUTPUT)$(SEP)test-fuzzydepth4-dupes.pgn
	$(CMP) test-fuzzydepth4-unique.pgn $(OUTPUT)$(SEP)test-fuzzydepth4-unique.pgn
	$(PGN_EXTRACT) -C --fuzzydepth 5 -dtest-fuzzydepth5-dupes.pgn -otest-fuzzydepth5-unique.pgn --quiet $(INPUT)$(SEP)test-fuzzydepth.pgn
	$(CMP) test-fuzzydepth5-dupes.pgn $(OUTPUT)$(SEP)test-fuzzydepth5-dupes.pgn
	$(CMP) test-fuzzydepth5-unique.pgn $(OUTPUT)$(SEP)test-fuzzydepth5-unique.pgn

# Selection or exclusion of games with SetUp tags.
#     + Input file with games involving SetUp tags.
#     - Input file(s): test-setup-in.pgn
#     - Game output should either have only games with SetUp tags
#       or only those without.
#     - Expected output: test-nosetup-out.pgn test-onlysetup-out.pgn
test-setup:
	echo "test-setup:"
	$(PGN_EXTRACT) --nosetuptags -otest-nosetup-out.pgn --quiet $(INPUT)$(SEP)test-setup-in.pgn
	$(CMP) test-nosetup-out.pgn $(OUTPUT)$(SEP)test-nosetup-out.pgn
	$(PGN_EXTRACT) --onlysetuptags -otest-onlysetup-out.pgn --quiet $(INPUT)$(SEP)test-setup-in.pgn
	$(CMP) test-onlysetup-out.pgn $(OUTPUT)$(SEP)test-onlysetup-out.pgn

# --stopafter
#     + Input file containing games.
#     - Input file(s): test-stopafter.pgn
#     - Resulting output should contain a single matched game.
#     - Expected output: test-stopafter-out.pgn
test-stopafter:
	echo "test-stopafter:"
	$(PGN_EXTRACT) -TpPetrosian --stopafter 1 -otest-stopafter-out.pgn --quiet $(INPUT)$(SEP)test-stopafter.pgn
	$(CMP) test-stopafter-out.pgn $(OUTPUT)$(SEP)test-stopafter-out.pgn

# --nobadresults
#     + Input file containing games with inconsistent or incomplete results.
#     - Input file(s): test-nobadresults.pgn
#     - Resulting output should contain only those games which are not
#       strictly inconsistent.
#     - Expected output: test-nobadresults-out.pgn
test-nobadresults:
	echo "test-nobadresults:"
	$(PGN_EXTRACT) --nobadresults -otest-nobadresults-out.pgn -ltest-nobadresults-log.txt --quiet $(INPUT)$(SEP)test-nobadresults.pgn
	$(CMP) test-nobadresults-out.pgn $(OUTPUT)$(SEP)test-nobadresults-out.pgn
#       $(CMP) test-nobadresults-log.txt $(OUTPUT)$(SEP)test-nobadresults-log.txt

# --allownullmoves
#     + Input file containing games with inconsistent or incomplete results.
#     - Input file(s): test-allownullmoves.pgn
#     - Resulting output should contain only those games which are not
#       strictly inconsistent.
#     - Expected output: test-allownullmoves-out.pgn
test-allownullmoves:
	echo "test-allownullmoves:"
	$(PGN_EXTRACT) --allownullmoves -otest-allownullmoves-out.pgn -ltest-allownullmoves-log.txt --quiet $(INPUT)$(SEP)test-allownullmoves.pgn
	$(CMP) test-allownullmoves-out.pgn $(OUTPUT)$(SEP)test-allownullmoves-out.pgn
#       $(CMP) test-allownullmoves-log.txt $(OUTPUT)$(SEP)test-allownullmoves-log.txt

# --matchplylimit
#     + Input file containing games.
#     - Input file(s): test-matchplylimit.pgn
#     - Resulting output should contain games whose number of moves (plies) are
#       limited at the specified ply limit.
#     - Expected output: test-matchplylimit-out.pgn
test-matchplylimit:
	echo "test-matchplylimit:"
	-$(RM) test-matchplylimit-out.pgn
	$(PGN_EXTRACT) -Hdb55d4fcaadc775e --matchplylimit 8 --markmatches match -atest-matchplylimit-out.pgn --quiet $(INPUT)$(SEP)test-matchplylimit.pgn
	$(PGN_EXTRACT) -Hdb55d4fcaadc775e --matchplylimit 9 --markmatches match -atest-matchplylimit-out.pgn --quiet $(INPUT)$(SEP)test-matchplylimit.pgn
	$(PGN_EXTRACT) -Hdb55d4fcaadc775e --matchplylimit 10 --markmatches match -atest-matchplylimit-out.pgn --quiet $(INPUT)$(SEP)test-matchplylimit.pgn
	$(PGN_EXTRACT) -Hdb55d4fcaadc775e --matchplylimit 150 --markmatches match -atest-matchplylimit-out.pgn --quiet $(INPUT)$(SEP)test-matchplylimit.pgn
	$(CMP) test-matchplylimit-out.pgn $(OUTPUT)$(SEP)test-matchplylimit-out.pgn

# -A/-t with FENPattern and FENPatternI
# Test both FENPattern and FENPatternI and add a MatchLabel tag
# if the pattern has an associated label.
test-FENPattern:
	echo "test-FENPattern:"
	$(PGN_EXTRACT) -otest-FENPattern-out.pgn -ltest-FENPattern-log.txt --quiet \
            -A$(INPUT)$(SEP)test-FENPattern.txt --addlabeltag --markmatches HERE \
            $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-FENPattern-out.pgn $(OUTPUT)$(SEP)test-FENPattern-out.pgn
	$(PGN_EXTRACT) -otest-FENPattern-cli-out.pgn -ltest-FENPattern-log.txt --quiet \
            --fenpattern "*/*/*/*/???pN???/???P????/*/*" \
            --fenpatterni "*/*/*/*/???Np???/????P???/*/*" \
            --markmatches HERE \
            $(INPUT)$(SEP)petrosian.pgn
	$(CMP) test-FENPattern-cli-out.pgn $(OUTPUT)$(SEP)test-FENPattern-cli-out.pgn
#       $(CMP) test-FENPattern-log.txt $(OUTPUT)$(SEP)test-FENPattern-log.txt

# --startply
#     + Input file containing games.
#     - Input file(s): test-startply.pgn
#     - Resulting output should contain games matching the given FEN pattern,
#       only starting the match at the specified ply.
#     - Expected output: test-startply-out.pgn
test-startply:
	echo "test-startply:"
	-$(RM) test-startply-out.pgn
	$(PGN_EXTRACT) --startply 8 -Tf"*/*/4p3/3pP3/2pP4/2P5/*/*" --markmatches match -atest-startply-out.pgn --quiet $(INPUT)$(SEP)test-startply.pgn
	$(PGN_EXTRACT) --startply 9 -Tf"*/*/4p3/3pP3/2pP4/2P5/*/*" --markmatches match -atest-startply-out.pgn --quiet $(INPUT)$(SEP)test-startply.pgn
	$(PGN_EXTRACT) --startply 10 -Tf"*/*/4p3/3pP3/2pP4/2P5/*/*" --markmatches match -atest-startply-out.pgn --quiet $(INPUT)$(SEP)test-startply.pgn
	$(PGN_EXTRACT) --startply 11 -Tf"*/*/4p3/3pP3/2pP4/2P5/*/*" --markmatches match -atest-startply-out.pgn --quiet $(INPUT)$(SEP)test-startply.pgn
	$(PGN_EXTRACT) --startply 100 -Tf"*/*/4p3/3pP3/2pP4/2P5/*/*" --markmatches match -atest-startply-out.pgn --quiet $(INPUT)$(SEP)test-startply.pgn
	$(PGN_EXTRACT) --startply 200 -Tf"*/*/4p3/3pP3/2pP4/2P5/*/*" --markmatches match -atest-startply-out.pgn --quiet $(INPUT)$(SEP)test-startply.pgn
	$(CMP) test-startply-out.pgn $(OUTPUT)$(SEP)test-startply-out.pgn

test-linenumbers:
	echo "test-linenumbers:"
	-$(RM) test-linenumbers-out.pgn
	$(PGN_EXTRACT) --quiet --linenumbers marker infiles/petrosian.pgn -o test-linenumbers-out.pgn
	$(CMP) test-linenumbers-out.pgn $(OUTPUT)$(SEP)test-linenumbers-out.pgn

test-ratedwins:
	echo "test-ratedwins:"
	$(PGN_EXTRACT) --quiet --higherratedwinner infiles/bots.pgn -o test-higherratedwins-out.pgn
	$(CMP) test-higherratedwins-out.pgn $(OUTPUT)$(SEP)test-higherratedwins-out.pgn
	$(PGN_EXTRACT) --quiet --lowerratedwinner infiles/bots.pgn -o test-lowerratedwins-out.pgn
	$(CMP) test-lowerratedwins-out.pgn $(OUTPUT)$(SEP)test-lowerratedwins-out.pgn

test-odds:
	echo "test-odds:"
	$(PGN_EXTRACT) --quiet --odds --allownullmoves infiles/odds.pgn -o odds-out.pgn
	$(CMP) odds-out.pgn $(OUTPUT)$(SEP)odds-out.pgn
