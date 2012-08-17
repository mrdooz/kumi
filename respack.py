import os, sys, argparse, tempfile, subprocess

class ResFile():
    def __init__(self, input_file, input_size, output_file, output_size):
        self.input_file = input_file
        self.input_size = input_size
        self.output_file = output_file
        self.output_size = output_size
        
parser = argparse.ArgumentParser()
parser.add_argument('input_file', metavar='I', help='input file')
parser.add_argument('output_file', metavar='O', help='output file')
args = parser.parse_args()

tmpdir = tempfile.gettempdir()

files = []

orgsize = 0
finalsize = 0
# compress each file
for src in [x.strip() for x in open(args.input_file).readlines()]:
    (head, tail) = os.path.split(src)
    dst = os.path.join(tmpdir) + tail + '.lz4'
    subprocess.call(['lz4', '-c2', src, dst])
    srcsize = os.path.getsize(src)
    dstsize = os.path.getsize(dst)
    
    orgsize += srcsize
    finalsize += dstsize
    
    files.append(ResFile(src, srcsize, dst, dstsize))

for f in files:
    os.remove(f.output_file)
    
print "org size: ", orgsize, "final size: ", finalsize