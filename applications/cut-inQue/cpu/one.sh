#!/bin/bash
#!/bin/sh

grep "rx:" output.txt > rx_output.txt
grep "fwd:" output.txt > fwd_output.txt
grep "txx:" output.txt > tx_output.txt
grep "cut:" output.txt > cut_output.txt
./cycleCal.py rx_output.txt fwd_output.txt tx_output.txt cut_output.txt