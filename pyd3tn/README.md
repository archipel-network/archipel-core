# pyD3TN

pyD3TN is a collection of protocol implementations of the *Delay-Tolerant
Networking (DTN) Architecture* ([RFC4838][rfc4838]) for Python.

Besides the *Bundle Protocol* implementations [BPv6][bpv6] and [BPv7][bpv7],
implementations of the *Convergence Layer Adapters* (CLAs) for TCP
([MTCP][mtcp], [TCPCLv3][tcpclv3]) and the Space Packet Protocol ([SPP][spp])
are included.

## Installation

From source:

```sh
git clone https://gitlab.com/d3tn/ud3tn
cd pyd3tn
python setup.py install
```

From [PyPi](https://pypi.org/project/pyD3TN/) directly:

```sh
pip install pyd3tn
```

## Development

pyD3TN is maintained as part of the µD3TN project and follows its development
processes. Please see the [µD3TN][ud3tn] repository for the code and further
information.

[rfc4838]: https://tools.ietf.org/html/rfc4838
[bpv6]: https://tools.ietf.org/html/rfc5050
[bpv7]: https://tools.ietf.org/html/draft-ietf-dtn-bpbis-25
[mtcp]: https://tools.ietf.org/html/draft-ietf-dtn-mtcpcl-00
[tcpclv3]: https://tools.ietf.org/html/rfc7242
[spp]: https://public.ccsds.org/Pubs/133x0b2e1.pdf
[ud3tn]: https://gitlab.com/d3tn/ud3tn
