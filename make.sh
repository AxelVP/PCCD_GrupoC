clear

rm salida*.txt
rm receptor
rm proceso_general
rm consulta
rm inicio

for var in 1, 2, 3, 4
do
    killall inicio
    killall receptor
    killall proceso_general
    killall consulta
done


gcc -Wall receptor.c -o receptor -lpthread
gcc -Wall proceso_general.c -o proceso_general -lpthread
gcc -Wall proceso_consultas.c -o consulta -lpthread
gcc -Wall inicio.c -o inicio -lpthread
