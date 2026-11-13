"""Read native-size 8-bit RGB PSD layers, respecting declared channel lengths."""
import io
import struct
from pathlib import Path
import zlib
from PIL import Image


def unpack_bits(data, expected):
    output = bytearray()
    cursor = 0
    while cursor < len(data):
        count = data[cursor]; cursor += 1
        if count < 128:
            size = count+1
            output.extend(data[cursor:cursor+size]); cursor += size
        elif count > 128:
            if cursor >= len(data): raise ValueError('truncated PSD RLE run')
            output.extend(bytes([data[cursor]])*(257-count)); cursor += 1
    if len(output) != expected: raise ValueError('PSD RLE row has wrong length')
    return bytes(output)


def layers(path):
    stream = io.BytesIO(Path(path).read_bytes())
    def read(count):
        value=stream.read(count)
        if len(value)!=count: raise ValueError('truncated PSD')
        return value
    def number(fmt): return struct.unpack('>'+fmt,read(struct.calcsize('>'+fmt)))[0]
    if read(4)!=b'8BPS' or number('H')!=1: raise ValueError('unsupported PSD signature/version')
    read(6); number('H'); number('I'); number('I')
    if number('H')!=8 or number('H')!=3: raise ValueError('only 8-bit RGB PSD files are supported')
    read(number('I')); read(number('I'))
    if not number('I') or not number('I'): return {}
    count=abs(number('h'))
    records=[]
    for _ in range(count):
        top,left,bottom,right=struct.unpack('>4i',read(16))
        channels=[(number('h'),number('I')) for _ in range(number('H'))]
        if read(4)!=b'8BIM': raise ValueError('invalid PSD blend signature')
        blend=read(4); opacity=number('B'); number('B'); flags=number('B'); number('B')
        extra=number('I'); end=stream.tell()+extra
        read(number('I')); read(number('I'))
        length=number('B'); name=read(length).decode('latin1')
        stream.seek(end)
        records.append(dict(name=name,bounds=(left,top,right,bottom),channels=channels,opacity=opacity,visible=not(flags&2),blend=blend))
    result={}
    for record in records:
        left,top,right,bottom=record['bounds']; width,height=right-left,bottom-top
        planes={}
        for channel,length in record['channels']:
            blob=read(length)
            if channel not in (0,1,2,-1) or not width or not height: continue
            compression=struct.unpack('>H',blob[:2])[0]
            if compression==0: pixels=blob[2:2+width*height]
            elif compression==1:
                sizes=struct.unpack('>'+str(height)+'H',blob[2:2+height*2])
                offset=2+height*2; rows=[]
                for size in sizes:
                    rows.append(unpack_bits(blob[offset:offset+size],width)); offset+=size
                pixels=b''.join(rows)
            elif compression==2: pixels=zlib.decompress(blob[2:])
            else: raise ValueError('unsupported PSD channel compression')
            if len(pixels)!=width*height: raise ValueError('PSD channel has wrong dimensions')
            planes[channel]=Image.frombytes('L',(width,height),pixels)
        if not width or not height: continue
        if any(channel not in planes for channel in (0,1,2)): raise ValueError('PSD layer lacks RGB channels')
        alpha=planes.get(-1,Image.new('L',(width,height),255))
        if record['opacity']!=255: alpha=alpha.point(lambda value:value*record['opacity']//255)
        record['image']=Image.merge('RGBA',tuple(planes[channel] for channel in (0,1,2))+(alpha,))
        result[record['name']]=record
    return result
