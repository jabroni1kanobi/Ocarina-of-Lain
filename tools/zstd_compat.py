"""The part of Python 3.14's compression.zstd the shader tools use, on older Pythons through the zstandard package.

decompress(data, zstd_dict=None), ZstdDict(raw) with .dict_id, and get_frame_info(data) with .decompressed_size (None
when the frame does not say) and .dictionary_id (0 for none). On 3.14 the standard module itself is used.
"""
try:
    from compression.zstd import ZstdDict, decompress, get_frame_info  # noqa: F401
except ImportError:
    from collections import namedtuple

    import zstandard

    FrameInfo = namedtuple('FrameInfo', 'decompressed_size dictionary_id')
    LIMIT = 1 << 30   # the largest output a frame without a content size may decompress to

    class ZstdDict:
        def __init__(self, raw):
            self._d = zstandard.ZstdCompressionDict(raw, dict_type=zstandard.DICT_TYPE_AUTO)
            self.dict_id = self._d.dict_id()

    def get_frame_info(data):
        p = zstandard.get_frame_parameters(data)
        size = p.content_size if p.content_size >= 0 and p.content_size != zstandard.CONTENTSIZE_UNKNOWN else None
        return FrameInfo(size, p.dict_id)

    def decompress(data, zstd_dict=None):
        d = zstandard.ZstdDecompressor(dict_data=zstd_dict._d if zstd_dict else None)
        p = zstandard.get_frame_parameters(data)
        if 0 <= p.content_size != zstandard.CONTENTSIZE_UNKNOWN:
            # one frame of a known size, the common case: decompressed in one call (the streaming path below is
            # several times slower on the shader archives' hundreds of megabytes)
            frame = zstandard.frame_content_size(data)
            if frame == p.content_size:
                try:
                    return d.decompress(data)
                except zstandard.ZstdError:
                    pass   # more than one frame: the streaming path
        out, pos = [], 0
        # One frame after another, as compression.zstd decompresses concatenated frames.
        while pos < len(data):
            obj = d.decompressobj()
            out.append(obj.decompress(data[pos:]))
            rest = obj.unused_data
            pos = len(data) - len(rest)
            if not rest:
                break
        return b''.join(out)
