#include "selector.hpp"
#include "selectorprovider.hpp"


Selector* Selector::open()
{
    return SelectorProvider::provider().openSelector();

}
