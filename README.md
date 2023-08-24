![Archipel core logo : 3 spikes above a wave](./doc/logo_banner.svg)

# Archipel Core - An experimental µD3TN fork

*Exploring Delay Tolerant Networks*

An experimental fork of µD3TN focused on the creation of a frugal delay tolerant network.

**Note:** This is a fork of original µD3TN released on [https://gitlab.com/d3tn/ud3tn](https://gitlab.com/d3tn/ud3tn). This fork is implementing custom routing algorithms and experimental features that doesn't need to be merged to the main codebase.

## Install

Make sure you have necessary tools for C building, python3, pip and python libraries `cbor` installed globally

On debian :

```
apt get install build-essential python3 python3-pip python3-cbor
```

Clone this repository (with optional `--depth 1` parameter for smaller repository size)

```sh-session
git clone https://github.com/EpicKiwi/archipel-core.git --depth 1 --recurse-submodules
```

Build

```sh-session
make posix
```

Install (as root)

```sh-session
make install-posix
```

Configure your node by copying `default-conf.env` to `/etc/archipel-core/conf.env` (as root)

```sh-session
mkdir -p /etc/archipel-core
cp default-conf.env /etc/archipel-core/conf.env
```

Edit `/etc/archipel-core/conf.env` and change `NODE_ID` for your own name on the network (as root)

Start system-wide service (as root)

```sh-session
systemctl start archipel-core
```

Archipel core is now installed on your machine.

### Optional features

Configure neighbour discovery by installing [archipel-ipbeacon](https://github.com/EpicKiwi/archipel-ipbeacon#readme)

Use USB drives to transmit bundle with [archipel-file-carrier](https://github.com/EpicKiwi/archipel-file-carrier)

## Development

### Sync with µD3TN

Add a new remote if you don't have any

```
git remote add ud3tn "https://gitlab.com/d3tn/ud3tn.git"
```

Go on the `ud3tn` and pull `master` branch from µD3TN repository

```
git checkout ud3tn
git pull ud3tn master
```

Don't forget to pull the result on this repository

```
git push origin ud3tn
```

## Related Projects

Here are some others experiments and projects around DTN and Archipel Core

* [archipel-ipbeacon](https://github.com/EpicKiwi/archipel-ipbeacon) Neighbour discovery
* [archipel-file-carrier](https://github.com/EpicKiwi/archipel-file-carrier) Use USB drives to transmit bundles
* [ud3tn-aap](https://github.com/EpicKiwi/rust-ud3tn) Rust implementation of ud3tn's aap protocol

## See also

- [RFC 4838](https://datatracker.ietf.org/doc/html/rfc4838) for a general introduction about DTN networks.
- [ION](https://sourceforge.net/projects/ion-dtn/): NASA's bundle protocol implementation that has been successfully demonstrated to be interoperable with µD3TN.
- [µD3TN](https://gitlab.com/d3tn/ud3tn)
