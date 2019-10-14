# PAGERANK README

# Graph500 Input
# Load data to FAM
../third-party/build/bin/mpirun -np 1 ./test/apps/pagerank/fam_csr_preload_graph500 config_20_4_8.txt

# Pagerank with rows per itertion 10k and number of iterations 5
../third-party/build/bin/mpirun -np 8 ./test/apps/pagerank/fam_pagerank_graph500 config_20_4_8.txt 10000 5

# Format of Config File
1048576 /*mat row count*/
8 /* segments */
/home/${USER}/graph500/src/out_20_4_8

#PARMAT Input
# Load data to FAM
../third-party/build/bin/mpirun -np 1 ./test/apps/pagerank/fam_csr_preload_parmat parmat_1M_4M.txt 1048576 4

# Pagerank with rows per itertion 1k and number of iterations 5
../third-party/build/bin/mpirun -np 8 ./test/apps/pagerank/fam_pagerank_parmat 1024 5

# Parmat command to generate matrix of size 1M x 1M with 4 non zero per row
./PaRMAT -nVertices 1048576 -nEdges 1048576*4 -sorted 
