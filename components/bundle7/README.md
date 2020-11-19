# Bundle Protocol v7 Parser

C implementation of the Bundle Protcol v7 as specified in the [IETF Draft].


## Dependencies

### µD3TN

The **bundle7** library requires some header files of the **ud3tn** library as
well as its generic bundle functions from `bundle.c`, `bundle_fragmenter.c*`
and a small part of its platform abstraction, namely `hal_time`.

### TinyCBOR

The [TinyCBOR] library is required for parsing and serializing bundle7 data.


[IETF Draft]: [https://tools.ietf.org/html/draft-ietf-dtn-bpbis-17]
[TinyCBOR]: https://github.com/intel/tinycbor/
