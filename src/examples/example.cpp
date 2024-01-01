#include <fixparser.h>
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

int main()
{

    std::string msg("8=FIX.4.4|9=148|35=D|34=1080|49=TESTBUY1|52=20180920-18:14:19.508|56=TESTSELL1|11=636730640278898634|15=USD|21=2|38=7000|40=1|54=1|55=MSFT|60=20180920-18:14:19.492|10=092|");
    fixparser::FixParser parser(msg);
    parser.pprint();

    fixparser::FixParser parser2(fs::path("samples/fix42_messages.fix"));
    parser2.pprint();

    return 0;
}