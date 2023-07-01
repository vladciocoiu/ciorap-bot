import random

lines = []
with open("10m-tuning-positions.txt", 'r') as f:
    lines = f.readlines()

with open("10m-tuning-positions-parsed.txt", 'w') as f:
    #shuffle lines
    random.shuffle(lines)
    for line in lines:
        f.write(' '
                .join(line.split()[:-1])
                .replace('[', '"')
                .replace(']', '"')
                .replace("\"0.0\"", "\"0-1\"")
                .replace("\"1.0\"", "\"1-0\"")
                .replace("\"0.5\"", "\"1/2-1/2\"")
                  + ';\n')
