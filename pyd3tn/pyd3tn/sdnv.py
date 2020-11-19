"""Self-Delimiting Numeric Values (RFC 6256)"""


def sdnv_encode(value):
    """Returns the SDNV-encoded byte string representing value

    Args:
        value (int): Non-negative integer that should be encoded
    Returns:
        bytes: SDNV-encoded number
    Raises:
        ValueError: If value not an integer or negative
    """
    if value < 0 or not isinstance(value, int):
        raise ValueError("Only non-negative integers can be SDNV-encoded")

    # Special case: zero whould produce an empty byte string
    if value == 0:
        return b"\x00"

    result = bytearray()
    while value != 0:
        result.append((value & 0x7f) | 0x80)
        value >>= 7

    # Clear MSB in the first byte that was set by the XOR with 0x80
    result[0] &= 0x7f

    # Network byte order
    return bytes(reversed(result))


def sdnv_decode(buffer):
    """Decodes a byte string (or any iterable over bytes) assumed to be a an
    SDNV and returns the non-negative integer representing the numeric value

    Args:
        buffer (bytes): Encoded SDNV
    Returns:
        int: Decoded non-negative integer
    Raises:
        ValueError: If the buffer contains insufficient bytes (not the complete
        SDNV)
    """
    n = 0
    for i, byte in enumerate(buffer, 1):
        n = (n << 7) | (byte & 0x7f)

        if byte >> 7 == 0:
            return n, i

    raise ValueError("Insufficient bytes")


def test_sdnv_encode():
    assert sdnv_encode(0xabc) == b"\x95\x3c"
    assert sdnv_encode(0x4234) == b"\x81\x84\x34"


def test_sdnv_decode():
    import pytest

    assert sdnv_decode(b"\xa4\x34") == (0x1234, 2)
    assert sdnv_decode(b"\xa4\x34\x00\x00") == (0x1234, 2)
    assert sdnv_decode(b"\x7f") == (0x7f, 1)

    with pytest.raises(ValueError):
        assert sdnv_decode(b"\xa4")
