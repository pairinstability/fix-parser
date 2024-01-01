FIX 4.4 message parser
===

Financial Information eXchange (FIX) is the de-facto messaging standard for pre-trade, trade, and post-trade communication, as well as for U.S. regulatory reporting. It is used to disseminate price and trade information among investment banks and broker-dealers.

this library parses FIX messages and displays them in a human-readable way.

XML of spec is in `spec`. FIX message samples are in `samples`. example usage is in `examples`.

![](img/example.png)


building
---

tested on ubuntu 22.04

deps:
- [pugixml](https://github.com/zeux/pugixml). install however, I installed `libpugixml-dev` with apt.

build with `scripts/build.sh`.


resources
---

- https://www.fixtrading.org/standards/fix-4-4/
- https://www.nyse.com/publicdocs/nyse/markets/nyse/NYSE_CCG_FIX_Sample_Messages.pdf
- https://bitnomial.com/docs/fix-common-messages/
- https://www.onixs.biz/fix-dictionary/4.4/fields_by_tag.html#


to do
---

- message validation
- unit tests
