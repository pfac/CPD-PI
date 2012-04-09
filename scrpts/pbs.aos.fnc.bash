#!/bin/bash
#
#	SeARCH Job Script
#
#PBS -l nodes=1:r201:ppn=4
#PBS -l walltime=2:00:00
#
#PBS -M pdrcosta90@gmail.com
#PBS -m bea
#PBS -e data/out/pbs.aos.fnc.err
#PBS -o data/out/pbs.aos.fnc.out
#
declare -a CASES;
declare -a MEASURES;

CASE="huge"
EXEC="polu.aos"
TIMER="fnctime"
RUNS=10

cd "$PBS_O_WORKDIR";

for (( i = 0 ; i < $RUNS ; ++i ));
do
	bin/${EXEC}.${TIMER} data/xml/${CASE}.param.xml;
done;