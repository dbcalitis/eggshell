from ctypes import (
    CDLL,
    POINTER,
    Structure,
    c_uint8,
    c_int,
    byref,
    cast,
    create_string_buffer,
    addressof,
)

import pytest
from tests import ROOT, compile


class Tokenizer(Structure):
    _fields_ = [
        ("input", POINTER(c_uint8)),
        ("current_byte", POINTER(c_uint8)),
        ("state", c_int),
    ]


class String(Structure):
    _fields_ = [
        ("data", POINTER(c_uint8)),
        ("length", c_int),
    ]


class Token(Structure):
    _fields_ = [
        ("type", c_int),
        ("string", String),
    ]

TOKEN_WORD = 3
TOKEN_PIPE = 7

@pytest.fixture
def tokenizer():
    source = ROOT / "tokenizer.c"
    compile(source)

    lib = CDLL(str(source.with_suffix(".so")))

    lib.tokenize.argtypes = [
        POINTER(Tokenizer),
        POINTER(Token),
    ]
    lib.tokenize.restype = c_int

    return lib


# Helpers
def make_tokenizer(source: bytes):
    # Keep this buffer alive while the tokenizer uses its pointers.
    input_buffer = create_string_buffer(source)

    input_pointer = cast(
        input_buffer,
        POINTER(c_uint8),
    )

    state = Tokenizer(
        input=input_pointer,
        current_byte=input_pointer,
        state=0,  # STATE_NEW_TOKEN
    )

    return state, input_buffer


def get_token_text(token: Token) -> bytes:
    if token.string.length <= 0:
        return b""

    return bytes(
        token.string.data[i]
        for i in range(token.string.length)
    )


# Tests
@pytest.mark.parametrize(
    "source, expected_text",
    [
        (b"hello", b"hello"),
        (b"World", b"World"),
        (b"echo", b"echo"),
        (b"variable123", b"variable123"),
        (b"7xz", b"7xz"),
    ],
)
def test_identifiers(tokenizer, source, expected_text):
    state, input_buffer = make_tokenizer(source)
    token = Token()

    result = tokenizer.tokenize(byref(state), byref(token))

    assert result == TOKEN_WORD
    assert get_token_text(token) == expected_text

def test_pipe(tokenizer):
    state, input_buffer = make_tokenizer(b"|")
    token = Token()

    result = tokenizer.tokenize(byref(state), byref(token))

    # Checking the text is not necessary
    assert result == TOKEN_PIPE
