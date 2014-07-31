#!/bin/bash 

let f=1

for y in $(seq 1982 2014); do
	echo $y
	for m in $(seq 100 100 1200); do
		for d in 0 10 20; do
			let day=${y}*10000+${m}+${d}
			convert ipv4_map_${day}.png -pointsize 198.5 label:"${y}" -gravity Center -append ordered/${f}.png
			mogrify -resize 1024x1080 ordered/${f}.png
			echo "${m} ${d}"
			let f=f+1
		done
	done
done

