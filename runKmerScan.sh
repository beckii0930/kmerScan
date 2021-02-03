# samtools view -h HG00512.pbmm.lra.bam | head -n 29 | samtools view -bS - > test.bam
# samtools index test.bam

#gcc kmerScan.c -o kmerScan HG00512.pbmm.lra.bam test.txt
#cc -std=c11 kmerScan.c -o kmerScan
make
./kmerScan /home/cmb-16/nobackups/mjc/data_download/HG00512/HG00512.pbmm.lra.bam test3.txt 4
#./kmerScan test2.bam test.txt 5
