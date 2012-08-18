import os, sys, argparse, tempfile, subprocess, struct, binascii, array

# The perfect hashing code taken from:
# Easy Perfect Minimal Hashing 
# By Steve Hanov. Released to the public domain.
#
# Loosely based on the Compress, Hash, and Displace algorithm by Djamal Belazzougui,
# Fabiano C. Botelho, and Martin Dietzfelbinger

# Calculates a distinct hash function for a given string. Each value of the
# integer d results in a different hash value.
def hash( d, str ):
    if d == 0: d = 0x01000193

    # Use the FNV algorithm from http://isthe.com/chongo/tech/comp/fnv/ 
    for c in str:
        d = ( (d * 0x01000193) ^ ord(c) ) & 0xffffffff;

    return d

# Computes a minimal perfect hash table using the given python dictionary. It
# returns a tuple (G, V). G and V are both arrays. G contains the intermediate
# table of values needed to compute the index of the value in V. V contains the
# values of the dictionary.
def CreateMinimalPerfectHash( dict ):
    size = len(dict)

    # Step 1: Place all of the keys into buckets
    buckets = [ [] for i in range(size) ]
    G = [0] * size
    values = [None] * size
    
    for key in dict.keys():
        buckets[hash(0, key) % size].append( key )

    # Step 2: Sort the buckets and process the ones with the most items first.
    buckets.sort( key=len, reverse=True )        
    for b in xrange( size ):
        bucket = buckets[b]
        if len(bucket) <= 1: break
        
        d = 1
        item = 0
        slots = []

        # Repeatedly try different values of d until we find a hash function
        # that places all items in the bucket into free slots
        while item < len(bucket):
            slot = hash( d, bucket[item] ) % size
            if values[slot] != None or slot in slots:
                d += 1
                item = 0
                slots = []
            else:
                slots.append( slot )
                item += 1

        G[hash(0, bucket[0]) % size] = d
        for i in range(len(bucket)):
            values[slots[i]] = dict[bucket[i]]

        if ( b % 1000 ) == 0:
            print "bucket %d\n" % (b),
            sys.stdout.flush()

    # Only buckets with 1 item remain. Process them more quickly by directly
    # placing them into a free slot. Use a negative value of d to indicate
    # this.
    freelist = []
    for i in xrange(size): 
        if values[i] == None: freelist.append( i )

    for b in xrange( b, size ):
        bucket = buckets[b]
        if len(bucket) == 0: break
        slot = freelist.pop()
        # We subtract one to ensure it's negative even if the zeroeth slot was
        # used.
        G[hash(0, bucket[0]) % size] = -slot-1 
        values[slot] = dict[bucket[0]]
        if ( b % 1000 ) == 0:
            print "bucket %d    r" % (b),
            sys.stdout.flush()

    return (G, values)        

# Look up a value in the hash table, defined by G and V.
def PerfectHashLookup( G, V, key ):
    d = G[hash(0,key) % len(G)]
    if d < 0: return V[-d-1]
    return V[hash(d, key) % len(V)]

def strip_header_file(src):
    valid_lines = filter(lambda x : x.startswith('//'), open(src, 'rt').readlines())
    dst = src + '_stripped'
    open(dst, 'wt').writelines(valid_lines)
    return dst

class ResFile():
    def __init__(self, input_file, input_size, output_file, output_size, file_offset):
        self.input_file = input_file
        self.input_size = input_size
        self.output_file = output_file
        self.output_size = output_size
        self.file_offset = file_offset
        
parser = argparse.ArgumentParser()
parser.add_argument('input_file', metavar='I', help='input file')
parser.add_argument('output_file', metavar='O', help='output file')
args = parser.parse_args()

tmpdir = tempfile.gettempdir()

# input files consist of a tab-separated tuple (given-file resolved-file)
input_files = [x.strip() for x in open(args.input_file).readlines()]
given_files = [x.split('\t')[0] for x in input_files]
resolved_files = [x.split('\t')[1] for x in input_files]
num_files = len(input_files)
(G, V) = CreateMinimalPerfectHash(dict(zip(given_files, [x for x in range(num_files)])))

g = array.array('i', G)
v = array.array('i', V)

header_format = 'i i'   # header_size num_files
file_header = 'i i i'   # offset compressed_size original_size
header_size = struct.calcsize(header_format) + len(g) + len(v) + \
    struct.calcsize(file_header) * num_files

out_file = open(args.output_file, 'wb')
out_file.write(struct.pack(header_format, header_size, num_files))
g.tofile(out_file)
v.tofile(out_file)

files = []
file_offset = 0
org_size, final_size = 0, 0

# compress each file
for src in resolved_files:
    (head, tail) = os.path.split(src)

    # strip all the unimportant cruft from .h files
    if tail.endswith('.h'):
        src = strip_header_file(src)

    dst = os.path.join(tmpdir, tail + '.lz4')
    subprocess.call(['lz4compr', src, dst])
    src_size = os.path.getsize(src)
    dst_size = os.path.getsize(dst)
    
    org_size += src_size
    final_size += dst_size

    files.append(ResFile(src, src_size, dst, dst_size, file_offset))
    file_offset += dst_size

# write the file headers
for f in files:
    out_file.write(struct.pack(file_header, f.file_offset, f.output_size, f.input_size))

# copy the file data    
for f in files:
    out_file.write(open(f.output_file, 'rb').read())

#for f in files:
#    os.remove(f.output_file)

print "tmp dir:", tmpdir
print "org size: ", org_size, "final size: ", final_size