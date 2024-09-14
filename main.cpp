#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <iomanip>
#include <ios>
#include <vector>
#include <tuple>
#include <type_traits>
#include <cassert>
#include <cmath>
#include <limits>
#include <random>
#include <chrono>

enum class ColumnFormat {
    AUTO,
    SCIENTIFIC,
    FIXED,
    PERCENT
};

template < class...Ts >
class Tabulator {
    public: typedef std::tuple < Ts... > RowData;

    Tabulator(std::vector < std::string > headers,
        unsigned int static_column_size = 0,
        unsigned int cell_padding = 1): _headers(headers),
    _num_columns(std::tuple_size < RowData > ::value),
    _static_column_size(static_column_size),
    _cell_padding(cell_padding) {
        assert(headers.size() == _num_columns);
    }

    void addRow(Ts...entries) {
        _data.emplace_back(std::make_tuple(entries...));
    }

    template < typename StreamType >
    void print(StreamType & stream) {
        computeColumnSizes();

        unsigned int total_width = _num_columns + 1;

        for (auto & col_size: _column_sizes)
            total_width += col_size + (2 * _cell_padding);

        stream << std::string(total_width, '-') << "\n";

        stream << "|";
        for (unsigned int i = 0; i < _num_columns; i++) {
            auto half = _column_sizes[i] / 2;
            half -= _headers[i].size() / 2;

            stream << std::string(_cell_padding, ' ') << std::setw(_column_sizes[i]) << std::left <<
                std::string(half, ' ') + _headers[i] << std::string(_cell_padding, ' ') << "|";
        }

        stream << "\n";

        stream << std::string(total_width, '-') << "\n";

        for (auto & row: _data) {
            stream << "|";
            printRow(row, stream);
            stream << "\n";
        }

        stream << std::string(total_width, '-') << "\n";
    }

    void setColumnFormat(const std::vector < ColumnFormat > & column_format) {
        assert(column_format.size() == std::tuple_size < RowData > ::value);
        _column_format = column_format;
    }

    void setColumnPrecision(const std::vector < int > & precision) {
        assert(precision.size() == std::tuple_size < RowData > ::value);
        _precision = precision;
    }

    protected: typedef decltype( & std::right) right_type;
    typedef decltype( & std::left) left_type;

    template < typename T,
    typename = typename std::enable_if <
    std::is_arithmetic < typename std::remove_reference < T > ::type > ::value > ::type >
    static right_type justify(int) {
        return std::right;
    }

    template < typename T >
    static left_type justify(long) {
        return std::left;
    }

    template < typename TupleType,
    typename StreamType >
    void printRow(TupleType && ,
        StreamType & ,
        std::integral_constant <
        size_t,
        std::tuple_size < typename std::remove_reference < TupleType > ::type > ::value > ) {}

    template < std::size_t I,
    typename TupleType,
    typename StreamType,
    typename = typename std::enable_if <
    I != std::tuple_size < typename std::remove_reference < TupleType > ::type > ::value > ::type >
    void printRow(TupleType && t, StreamType & stream, std::integral_constant < size_t, I > ) {
        auto & val = std::get < I > (t);

        if (!_precision.empty()) {
            assert(_precision.size() ==
                std::tuple_size < typename std::remove_reference < TupleType > ::type > ::value);

            stream << std::setprecision(_precision[I]);
        }

        if (!_column_format.empty()) {
            assert(_column_format.size() ==
                std::tuple_size < typename std::remove_reference < TupleType > ::type > ::value);

            if (_column_format[I] == ColumnFormat::SCIENTIFIC)
                stream << std::scientific;

            if (_column_format[I] == ColumnFormat::FIXED)
                stream << std::fixed;

            if (_column_format[I] == ColumnFormat::PERCENT)
                stream << std::fixed << std::setprecision(2);
        }

        stream << std::string(_cell_padding, ' ') << std::setw(_column_sizes[I]) <<
            justify < decltype(val) > (0) << val << std::string(_cell_padding, ' ') << "|";

        if (!_column_format.empty()) {
            stream.unsetf(std::ios_base::floatfield);
        }

        printRow(std::forward < TupleType > (t), stream, std::integral_constant < size_t, I + 1 > ());
    }

    template < typename TupleType,
    typename StreamType >
    void printRow(TupleType && t, StreamType & stream) {
        printRow(std::forward < TupleType > (t), stream, std::integral_constant < size_t, 0 > ());
    }

    template < class T >
    size_t computeSize(const T & data, decltype(((T * ) nullptr) -> size()) * = nullptr) {
        return data.size();
    }

    template < class T >
    size_t computeSize(const T & data,
        typename std::enable_if < std::is_integral < T > ::value > ::type * = nullptr) {
        if (data == 0)
            return 1;

        return data < 0 ? std::log10(data * (-1)) + 1 : std::log10(data) + 1;
    }

    size_t computeSize(...) {
        return _static_column_size;
    }

    template < typename TupleType >
    void determineSizes(TupleType && ,
        std::vector < size_t > & ,
        std::integral_constant <
        size_t,
        std::tuple_size < typename std::remove_reference < TupleType > ::type > ::value > ) {}

    template < std::size_t I,
    typename TupleType,
    typename = typename std::enable_if <
    I != std::tuple_size < typename std::remove_reference < TupleType > ::type > ::value > ::type >
    void determineSizes(TupleType && t, std::vector < size_t > & sizes, std::integral_constant < size_t, I > ) {
        sizes[I] = computeSize(std::get < I > (t));

        if (!_column_format.empty())
            if (_column_format[I] == ColumnFormat::PERCENT)
                sizes[I] = 6;

        determineSizes(std::forward < TupleType > (t), sizes, std::integral_constant < size_t, I + 1 > ());
    }

    template < typename TupleType >
    void determineSizes(TupleType && t, std::vector < size_t > & sizes) {
        determineSizes(std::forward < TupleType > (t), sizes, std::integral_constant < size_t, 0 > ());
    }

    void computeColumnSizes() {
        _column_sizes.resize(_num_columns);

        std::vector < size_t > column_sizes(_num_columns);

        for (unsigned int i = 0; i < _num_columns; i++)
            _column_sizes[i] = _headers[i].size();

        for (auto & row: _data) {
            determineSizes(row, column_sizes);

            for (unsigned int i = 0; i < _num_columns; i++)
                _column_sizes[i] = std::max(_column_sizes[i], column_sizes[i]);
        }
    }

    std::vector < std::string > _headers;
    unsigned int _num_columns;
    unsigned int _static_column_size;
    unsigned int _cell_padding;
    std::vector < RowData > _data;
    std::vector < size_t > _column_sizes;
    std::vector < ColumnFormat > _column_format;
    std::vector < int > _precision;
};

namespace Random
{
	static std::random_device g_RandomDevice;
	static bool				  g_IsDeviceInitialized = false;
	static std::mt19937		  g_Generator;

	inline int32_t Gen(int32_t min, int32_t max)
	{
		if (!g_IsDeviceInitialized)
		{
			std::mt19937::result_type seed =
				g_RandomDevice() ^ ((std::mt19937::result_type)std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::system_clock::now().time_since_epoch())
							.count() +
						(std::mt19937::result_type)std::chrono::duration_cast<std::chrono::microseconds>(
							std::chrono::high_resolution_clock::now().time_since_epoch())
							.count());

			g_Generator.seed(seed);
			g_IsDeviceInitialized = true;
		}

		std::uniform_int_distribution<int32_t> distrib(min, max);
		return distrib(g_Generator);
	}

	inline bool Gen(double chanceOfTrue) 
	{
		int random = Gen(0, 100);
		return random < chanceOfTrue;
	}
} 

namespace Text {

    inline void RemoveExtraSpaces(std::string &str) {
	
		str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](int c) { return !std::isspace(c); }));
		str.erase(std::find_if(str.rbegin(), str.rend(), [](int c) { return !std::isspace(c); }).base(), str.end());
		auto last =
			std::unique(str.begin(), str.end(), [](char a, char b) { return std::isspace(a) && std::isspace(b); });
		str.erase(last, str.end());
	}

	inline void RemoveSpaces(std::string &str) { 
		str.erase(remove(str.begin(), str.end(), ' '), str.end());
	}

    inline bool HasText(const std::string& mainString, const std::string& substring, bool caseSensitive = true) {
        
        if (!caseSensitive) {
            std::string mainStringCopy = mainString;
            std::string substringCopy = substring;
            Text::RemoveSpaces(mainStringCopy);
            Text::RemoveSpaces(substringCopy);
            std::transform(mainStringCopy.begin(), mainStringCopy.end(), mainStringCopy.begin(), ::tolower);
            std::transform(substringCopy.begin(), substringCopy.end(), substringCopy.begin(), ::tolower);
            size_t pos = mainStringCopy.find(substringCopy);
            return pos != std::string::npos;
        }
        
        size_t pos = mainString.find(substring);
        return pos != std::string::npos;
    }

    inline bool StartsWithChar(std::string& str, char what) {
		if (str.find(what) == 0)
			return true;

		return false;
	}

	inline bool StartsWithChar(std::string& str, std::string what) {
		if (str.find(str) == 0)
			return true;

		return false;
	}

	inline bool StartsWithString(const std::string& str, const std::string& prefix) {
		if (str.length() < prefix.length()) {
			return false; 
		}

		return str.substr(0, prefix.length()) == prefix;
	}
}

enum class SortOrder {
    ASCENDING,
    DESCENDING
};

enum class SortType {
    PRICE,
    STOCK_AMOUNT,
    ID,
};

class Product {
    public:
    Product() {
        m_ID = 0;
        m_Price = 0;
        m_StockAmount = 0;
        m_Name = "";
        m_Description = "";
    }

    ~Product() {
    }

    int getID() {
        return m_ID;
    }

    void setID(int ID) {
        m_ID = ID;
    }

    int getPrice() {
        return m_Price;
    }

    void setPrice(int price) {
        m_Price = price;
    }

    int getStockAmount() {
        return m_StockAmount;
    }

    void setStockAmount(int stockAmount) {
        m_StockAmount = stockAmount;
    }

    const char* getName() {
        return m_Name.c_str();
    }

    void setName(const char* name) {
        m_Name = name;
    }

    const char* getDescription() {
        return m_Description.c_str();
    }

    void setDescription(const char* description) {
        m_Description = description;
    }

    private:
    int m_ID;
    int m_Price;
    int m_StockAmount;
    std::string m_Name;
    std::string m_Description;
};

class ProductManager {
    
    public:
    ProductManager()  {
        m_LastProductID = 0;   
    }

    ~ProductManager() {
        for(Product* product : m_Products) {
            delete product;
        }
    }

    Product* getProduct(int ID) {
        for (Product* product : m_Products) {
            if (product->getID() == ID) {
                return product;
            }
        }

        return nullptr;
    }

    Product* getProductByName(const char* name) {
        for(auto product : m_Products) {
            if(Text::HasText(product->getName(), name)) {
                return product;
            }
        }
        return nullptr;
    }

    std::vector<Product*> getProductsWithString(const char* name) {
        std::vector<Product*> products;
        for(auto product : m_Products) {
            if(Text::StartsWithString(product->getName(), name)) {
                products.push_back(product);
            }
        }

        for(auto product : m_Products) {
            if(Text::HasText(product->getName(), name)) {
                if (std::find(products.begin(), products.end(), product) != products.end()) {
                    continue;
                }
                products.push_back(product);
            }
        }

        return products;
    }

    void sortProducts(SortType sortType, SortOrder sortOrder) {
        switch(sortType) {
            case SortType::PRICE: {
                std::sort(m_Products.begin(), m_Products.end(), [sortOrder](Product* a, Product* b) {
                    if(sortOrder == SortOrder::ASCENDING) {
                        return a->getPrice() < b->getPrice();
                    } else {
                        return a->getPrice() > b->getPrice();
                    }
                });
                break;
            }
            case SortType::STOCK_AMOUNT: {
                std::sort(m_Products.begin(), m_Products.end(), [sortOrder](Product* a, Product* b) {
                    if(sortOrder == SortOrder::ASCENDING) {
                        return a->getStockAmount() < b->getStockAmount();
                    } else {
                        return a->getStockAmount() > b->getStockAmount();
                    }
                });
                break;
            }
            case SortType::ID: {
                std::sort(m_Products.begin(), m_Products.end(), [sortOrder](Product* a, Product* b) {
                    if(sortOrder == SortOrder::ASCENDING) {
                        return a->getID() < b->getID();
                    } else {
                        return a->getID() > b->getID();
                    }
                });
                break;
            }
            default: {
                std::cout << "Invalid sort type" << std::endl;
                break;
            }
        }
    }

    void addProduct(Product* product) {
        product->setID(getLastProductID(true));
        m_Products.push_back(product);
    }

    void removeProduct(int ID) {
        m_Products.erase(m_Products.begin() + ID);
    }

    void initDefaults() {
        {
            Product* product = new Product();
            product->setName("Apple");
            product->setDescription("A fruit that is red and green");
            product->setPrice(10);
            product->setStockAmount(100);
            addProduct(product);
        }

        {
            Product* product = new Product();
            product->setName("Banana");
            product->setDescription("A fruit that is yellow");
            product->setPrice(7);
            product->setStockAmount(50);
            addProduct(product);
        }

        {
            Product* product = new Product();
            product->setName("Orange");
            product->setDescription("A fruit that is orange");
            product->setPrice(15);
            product->setStockAmount(25);
            addProduct(product);
        }

        {
            Product* product = new Product();
            product->setName("Grape");
            product->setDescription("A fruit that is purple");
            product->setPrice(12);
            product->setStockAmount(10);
            addProduct(product);
        }

        {
            Product* product = new Product();
            product->setName("Pineapple");
            product->setDescription("A fruit that is yellow and green");
            product->setPrice(30);
            product->setStockAmount(5);
            addProduct(product);
        }
    }

    int getLastProductID(bool increment = false) {
        return m_LastProductID += (increment ? 1 : 0);
    }

    std::vector<Product*> getProducts() {
        return m_Products;
    } 

    void printProducts() {
        for(auto product : m_Products) {

            if (!product) {
                continue;
            }

            std::cout << "Product ID: " << product->getID() << std::endl;
            std::cout << "Product Name: " << product->getName() << std::endl;
            std::cout << "Product Price: " << product->getPrice() << std::endl;
            std::cout << "Product Stock Amount: " << product->getStockAmount() << std::endl;
            std::cout << "Product Description: " << product->getDescription() << std::endl;
            std::cout << std::endl;
        }
    }

    private:
    std::vector<Product*> m_Products;
    int m_LastProductID;
};

ProductManager g_ProductManager = ProductManager();

class Order {

    public:
    Order() {
        m_IsCheckedOut = false;
        m_ProductID = 0;
        m_OrderID = 0;
        m_Quantity = 0;
        m_ShippingCost = 0;
    }

    ~Order() {
    }

    bool isCheckedOut() {
        return m_IsCheckedOut;
    }

    void setCheckedOut(bool checkedOut) {
        m_IsCheckedOut = checkedOut;
    }

    int getProductID() {
        return m_ProductID;
    }

    void setProductID(int productID) {
        m_ProductID = productID;
    }

    int getOrderID() {
        return m_OrderID;
    }

    void setOrderID(int orderID) {
        m_OrderID = orderID;
    }

    int getQuantity() {
        return m_Quantity;
    }

    void setQuantity(int quantity) {
        m_Quantity = quantity;
    }

    int getShippingCost() {
        return m_ShippingCost;
    }

    void setShippingCost(int shippingCost) {
        m_ShippingCost = shippingCost;
    }

    int getProductCost() {
        Product* product = g_ProductManager.getProduct(m_ProductID);
        if (!product) {
            return 0;
        }
        return product->getPrice();
    }

    int getTotalCost() {
        return (getProductCost() * getQuantity()) + m_ShippingCost;
    }

    std::string getProductName() {
        Product* product = g_ProductManager.getProduct(m_ProductID);
        if (!product) {
            return "";
        }
        return product->getName();
    }

    private:
    bool m_IsCheckedOut;
    int m_ProductID;
    int m_OrderID;
    int m_Quantity;
    int m_ShippingCost;
};

class Orders {  
    public:
    Orders() {
        m_Orders = {};
        m_LastOrderID = 0;
    }

    ~Orders() {
    }

    void addOrder(Order* order) {
        order->setOrderID(getLastOrderID(true));
        m_Orders.push_back(order);
    }

    void removeOrder(int orderID) {
        m_Orders.erase(m_Orders.begin() + orderID);
    }

    Order* getOrder(int orderID) {
        return m_Orders[orderID];
    }

    int getLastOrderID(bool increment = false) {
        return m_LastOrderID += (increment ? 1 : 0);
    }

    int size() {
        return m_Orders.size();
    }

    private:
    std::vector<Order*> m_Orders;
    int m_LastOrderID;
};

Orders g_Orders = Orders();

class ShoppingCart {

    public:
    ShoppingCart() {
        m_Cart = {};
    }

    ~ShoppingCart() {
    }

    bool addProductToCart(Product* product, int quantity) {
        if (product->getStockAmount() < quantity) {
            std::cout << "Not enough stock\n";
            return false;
        }

        Order* order = new Order();
        order->setProductID(product->getID());
        order->setQuantity(quantity);
        m_Cart.push_back(order);

        return true;
    }

    void removeProductFromCart(int productID) {
        m_Cart.erase(std::remove_if(m_Cart.begin(), m_Cart.end(), [productID](Order* order) {
            return order->getProductID() == productID;
        }), m_Cart.end());
    }

    void clearCart() {
        m_Cart.clear();
    }

    int getCartSize() {
        return m_Cart.size();
    }

    Order* getOrder(int index) {
        return m_Cart[index];
    }

    int getTotalCost() {
        int totalCost = 0;
        for(auto order : m_Cart) {
            totalCost += order->getTotalCost();
        }
        return totalCost;
    }

    void checkout() {
        for(Order* order : m_Cart) {
            order->setCheckedOut(true);
            order->setShippingCost(Random::Gen(10, 100));
            g_Orders.addOrder(order);
        }

        clearCart();
    }

    int getTotalProductCost() {
        int totalCost = 0;
        for(auto order : m_Cart) {
            totalCost += order->getProductCost();
        }
        return totalCost;
    }

    std::vector<Order*>& getCart() {
        return m_Cart;
    }

    private:
    std::vector<Order*> m_Cart;
};

ShoppingCart g_ShoppingCart = ShoppingCart();

void clear() {
    std::cout << "\033[2J\033[1;1H";
}


void showProductCatalog()
{
    clear();

    std::cout << "Product Catalog (" << g_ProductManager.getProducts().size() << ")\n";

    Tabulator<int, std::string, int, int, std::string> tabulator({"ID", "Name", "Price", "Stock Amount", "Description"});
    tabulator.setColumnFormat({ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO});

    for (Product* product : g_ProductManager.getProducts()) {
        tabulator.addRow(product->getID(), product->getName(), product->getPrice(), product->getStockAmount(), product->getDescription());        
    }

    tabulator.print(std::cout);

    std::cout << "What would you like to do?\n";
    std::cout << "1 - Sort Products\n";
    std::cout << "2 - Add Product to Cart\n";
    std::cout << "3 - Back\n";

    int choice;
    std::cin >> choice;

    switch(choice) {
        case 1: {
            std::cout << "Sort by:\n";
            std::cout << "1 - Price\n";
            std::cout << "2 - Stock Amount\n";
            std::cout << "3 - ID\n";

            int sortChoice;
            std::cin >> sortChoice;

            std::cout << "Sort order:\n";
            std::cout << "1 - Ascending\n";
            std::cout << "2 - Descending\n";

            int sortOrder;
            std::cin >> sortOrder;

            SortType sortType;
            switch(sortChoice) {
                case 1: {
                    sortType = SortType::PRICE;
                    break;
                }
                case 2: {
                    sortType = SortType::STOCK_AMOUNT;
                    break;
                }
                case 3: {
                    sortType = SortType::ID;
                    break;
                }
                default: {
                    std::cout << "Invalid sort type\n";
                    break;
                }
            }

            SortOrder order;
            switch(sortOrder) {
                case 1: {
                    order = SortOrder::ASCENDING;
                    break;
                }
                case 2: {
                    order = SortOrder::DESCENDING;
                    break;
                }
                default: {
                    std::cout << "Invalid sort order\n";
                    break;
                }
            }

            g_ProductManager.sortProducts(sortType, order);
            showProductCatalog();
            break;
        }
        case 2: {
            std::cout << "Enter product id: ";

            int productID;
            std::cin >> productID;

            Product* product = g_ProductManager.getProduct(productID);
            if (!product) {
                std::cout << "Invalid product id\n";
                break;
            }

            again:

            std::cout << "Enter quantity: ";
            int quantity;
            std::cin >> quantity;

            if (!g_ShoppingCart.addProductToCart(product, quantity))
            {
                goto again;
                break;
            }

            std::cout << "Product added to cart\n";
            break;
        }
        case 3: {
            break;
        }
        default: {
            std::cout << "Invalid choice\n";
            break;
        }
    }
}

void showShoppingCart()
{
    clear();

    std::cout << "Shopping Cart (" << g_ShoppingCart.getCartSize() << ")\n";

    Tabulator<int, std::string, int, int, int, int> tabulator({"ID", "Name", "Price", "Quantity", "Product Cost", "Total Cost"});
    tabulator.setColumnFormat({ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO});

    for (Order* order : g_ShoppingCart.getCart()) {
        Product* product = g_ProductManager.getProduct(order->getProductID());
        if (!product) {
            continue;
        }
        tabulator.addRow(product->getID(), product->getName(), product->getPrice(), order->getQuantity(), order->getProductCost(), order->getTotalCost());
    }

    tabulator.print(std::cout);

    std::cout << "Total Product Cost: " << g_ShoppingCart.getTotalProductCost() << std::endl;
    std::cout << "Total Cost: " << g_ShoppingCart.getTotalCost() << std::endl;

    std::cout << "What would you like to do?\n";
    std::cout << "1 - Checkout\n";
    std::cout << "2 - Remove Product\n";
    std::cout << "3 - Back\n";

    int choice;
    std::cin >> choice;

    switch(choice) {
        case 1: {
            g_ShoppingCart.checkout();
            std::cout << "Checkout successful\n";
            break;
        }
        case 2: {
            std::cout << "Enter product id: ";
            int productID;
            std::cin >> productID;

            g_ShoppingCart.removeProductFromCart(productID);
            std::cout << "Product removed from cart\n";
            break;
        }
        case 3: {
            break;
        }
        default: {
            std::cout << "Invalid choice\n";
            break;
        }
    }
}

void showPendingOrders()
{
    clear();

    std::cout << "Pending Orders (" << g_Orders.size() << ")\n";

    Tabulator<int, int, std::string, int, int, int, int> tabulator({"Order ID", "Product ID", "Name", "Quantity", "Shipping Cost", "Product Cost", "Total Cost"});
    tabulator.setColumnFormat({ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO, ColumnFormat::AUTO});

    for (int i = 0; i < g_Orders.getLastOrderID(); i++) {
        Order* order = g_Orders.getOrder(i);
        if (!order) {
            continue;
        }
        tabulator.addRow(order->getOrderID(), order->getProductID(), order->getProductName(), order->getQuantity(), order->getShippingCost(), order->getProductCost(), order->getTotalCost());
    }

    tabulator.print(std::cout);

    std::cout << "What would you like to do?\n";
    std::cout << "1 - Remove Order\n";
    std::cout << "2 - Back\n";

    int choice;
    std::cin >> choice;

    switch(choice) {
        case 1: {
            std::cout << "Enter order id: ";
            int orderID;
            std::cin >> orderID;

            g_Orders.removeOrder(orderID);
            std::cout << "Order removed\n";
            break;
        }
        case 2: {
            break;
        }
        default: {
            std::cout << "Invalid choice\n";
            break;
        }
    }
}

bool showMenu() {
    std::cout << "What would you like to do?\n";
    std::cout << "1 - View Product Catalog\n";
    std::cout << "2 - View Shopping Cart\n";
    std::cout << "3 - View Pending Orders\n";
    std::cout << "4 - Exit\n";
    std::cout << "Enter choice: ";

    int choice;
    std::cin >> choice;

    switch(choice) {
        case 1: {
            showProductCatalog();
            break;
        }
        case 2: {
            showShoppingCart();
            break;
        }
        case 3: {
            showPendingOrders();
            break;
        }
        case 4: {
            return false;
        }
        default: {
            std::cout << "Invalid choice\n";
            break;
        }
    }

    return true;
}

int main() {

    clear();

    std::cout << "Welcome to Coffee's Online Store\n";
    g_ProductManager.initDefaults();

    while(showMenu()) {}

    std::cout << "Thank you for shopping at Coffee's Online Store\n"
                 "See you again soon!\n\n";
    return 0;
}