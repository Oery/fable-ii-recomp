set pagination off
set confirm off
set debuginfod enabled off
break rex::codegen::phases::Validate
run
python
from pathlib import Path
ctx = gdb.parse_and_eval('ctx')
binary = ctx['binary_']
base = int(binary['baseAddress_'])
size = int(binary['imageSize_'])
memory = gdb.parse_and_eval('rex::memory::active_memory_')
host = int(memory['virtual_membase_']) + base
data = bytes(gdb.selected_inferior().read_memory(host, size))
assert data[:2] == b'MZ', 'Mapped image is not a PE image'
Path('tooling/guest-image.bin').write_bytes(data)
print(f'Saved SDK-loaded image: guest base {base:#x}, size {size:#x}')
end
quit
