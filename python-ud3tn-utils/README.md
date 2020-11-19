# python-uD3TN-utils

The Python package uD3TN-utils is a utility library to simplify the interaction
with the uD3TN daemon within python applications.

The included `AAPClient` enables user-friendly communication with the uD3TN
daemon via local or remote sockets using the Application Agent Protocol (AAP).
Besides sending and receiving bundles, it is also possible to change the
configuration of the uD3TN daemon via AAP messages.

## Installation

From source:

```sh
git clone https://gitlab.com/d3tn/ud3tn
cd python-ud3tn-utils
python setup.py install
```

From [PyPi](https://pypi.org/project/ud3tn-utils) directly:

```sh
pip install ud3tn-utils
```

## Examples

```python
from ud3tn_utils.aap import AAPUnixClient

with AAPUnixClient() as aap_client:
    aap_client.register()
    aap_client.send_str(aap_client.eid(), "Hello World")
    print(aap_client.receive())
```

## Development

pyD3TN is maintained as part of the µD3TN project and follows its development
processes. Please see the [µD3TN][ud3tn] repository for the code and further
information.

[ud3tn]: https://gitlab.com/d3tn/ud3tn
