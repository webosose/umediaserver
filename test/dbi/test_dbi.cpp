#define BOOST_TEST_MODULE TestDBI
#include <boost/test/included/unit_test.hpp>
#include <list>
#include <dbi.h>

using namespace uMediaServer::DBI;
typedef SQLiteDBI sql_t;

const std::string db_init = R"__(
	drop table if exists books;
	create table books (
			id          varchar primary key,
			author      varchar,
			title       varchar,
			pages       integer,
			weight      float,
			instock     integer);
	insert into books values('kobzar', 'Taras Shevchenko', 'Kobzar', 452, 1.5, 1);
	insert into books values('mdick', 'Herman Melville', 'Moby-Dick', 927, 3.2, 0);
)__";
const std::string db_uri = ":memory:";

struct DbiFixture {
	DbiFixture() : sql(db_uri) {
		sql << db_init;
	}
	sql_t sql;
};

namespace test {

struct book {
	std::string id;
	std::string author;
	std::string title;
	size_t      pages;
	float       weight;
	bool        instock;
};

}

void report_error(std::ostream & s, const uMediaServer::DBI::Exception & e) {
	using namespace uMediaServer::DBI;
	s << *boost::get_error_info<boost::throw_file>(e)
	  << "(" << *boost::get_error_info<boost::throw_line>(e) << "): "
	  << "<" << *boost::get_error_info<boost::throw_function>(e) << ">: ";
	if (auto * info_ptr = boost::get_error_info<throw_db_error>(e))
		s << *info_ptr << " ";
	if (auto * info_ptr = boost::get_error_info<throw_db_uri>(e))
		s << "{" << *info_ptr << "} ";
	if (auto * info_ptr = boost::get_error_info<throw_typeid>(e))
		s << "{" << *info_ptr << "} ";
	s << std::endl;
}

BOOST_FIXTURE_TEST_CASE(dbi_exceptions, DbiFixture) {

	// wrong database uri
	using namespace uMediaServer::DBI;
	BOOST_CHECK_THROW(sql_t("/fake_dir/fake_file"), OpenError);

	// sql syntax error
	BOOST_CHECK_THROW(sql << "select some hiking boots from books;"
						  << "drop table 'books';", ExecError);

	// wrong table name
	BOOST_CHECK_THROW(sql << "select * from 'book';", ExecError);

	// into type mismatch
	std::list<int> titles;
	BOOST_CHECK_THROW((sql << "select title from 'books';", into(titles)), ConvError);

	// binding type mismatch
	// FIXME: sqlite silently accepts type mismatch while binding
//	int id = 10; std::string title;
//	BOOST_CHECK_THROW((sql << "select title from 'books' where id=?;",
//						   from(id), into(title)), ConvError);

	// not enough storage provided
	std::string id("kobzar"), title, author;
	BOOST_CHECK_THROW((sql << "select title, author, weight from 'books' where id=?;",
						   from(id), into(author), into(title)), RangeError);

	id = "CAD"; author = "Ian Cain"; title = "Corporate America for Dummies";
	BOOST_CHECK_THROW((sql << "insert into 'books' values(?, ?, ?, ?, ?, ?);",
						   from(id), from(author), from(title)), RangeError);

	// error reporting
	try {
		sql << "errorneus sql query;";
	} catch (const Exception & e) {
		report_error(std::cerr, e);
	}
	try {
		int title;
		sql << "select title from 'books' where id='kobzar';", into(title);
	} catch (const Exception & e) {
		report_error(std::cerr, e);
	}
	try {
		std::string data;
		sql << "select title, author from 'books' where id='kobzar';", into(data);
	} catch (const Exception & e) {
		report_error(std::cerr, e);
	}
}

BOOST_FIXTURE_TEST_CASE(select_trivial_types, DbiFixture) {
	float weight;
	std::string author, title;
	size_t pages;
	bool instock;

	sql << "select author, title, pages, weight, instock from books where id='kobzar';",
			into(author), into(title), into(pages), into(weight), into(instock);

	BOOST_CHECK_EQUAL(author, "Taras Shevchenko");
	BOOST_CHECK_EQUAL(title, "Kobzar");
	BOOST_CHECK_EQUAL(pages, 452);
	BOOST_CHECK_EQUAL(weight, 1.5);
	BOOST_CHECK_EQUAL(instock, true);
}

BOOST_FIXTURE_TEST_CASE(insert_trivial_types, DbiFixture) {
	double weight = 0.8;
	std::string id("c++"), author("Bjarne Stroustrup"), title("A Tour of C++");
	size_t pages = 192;
	bool instock = false;

	sql << "insert into books values(?, ?, ?, ?, ?, ?);",
			from(id), from(author), from(title), from(pages), from(weight), from(instock);

	weight = 0.0; id.clear(); author.clear(); title.clear(); pages = 0; instock = true;

	sql << "select author, title, pages, weight, instock from books where id='c++';",
			into(author), into(title), into(pages), into(weight), into(instock);

	BOOST_CHECK_EQUAL(author, "Bjarne Stroustrup");
	BOOST_CHECK_EQUAL(title, "A Tour of C++");
	BOOST_CHECK_EQUAL(pages, 192);
	BOOST_CHECK_EQUAL(weight, 0.8);
	BOOST_CHECK_EQUAL(instock, false);
}

#include <boost/fusion/adapted.hpp>

BOOST_FUSION_ADAPT_STRUCT (
		test::book,
		(std::string, id)
		(std::string, author)
		(std::string, title)
		(size_t, pages)
		(float, weight)
		(bool, instock))

BOOST_FIXTURE_TEST_CASE(composite_types, DbiFixture) {
	test::book book;
	test::book expected{"CAD", "Ian Cain", "Corporate America for Dummies", 235, 1.3, true};
	size_t row_count;

	sql << "insert into books values(?, ?, ?, ?, ?, ?);", from(expected);
	sql << "select count(*) from books;", into(row_count);
	BOOST_CHECK_EQUAL(row_count, 3);

	sql << "select * from books where id=?;", from(expected), into(book);
	BOOST_CHECK(boost::fusion::equal_to(book, expected));

	sql << "delete from books where id=?;", from(book);
	sql << "select count(*) from books;", into(row_count);
	BOOST_CHECK_EQUAL(row_count, 2);
}

BOOST_FIXTURE_TEST_CASE(list_select, DbiFixture) {
	std::list<std::string> strings;
	std::list<std::string> expected{"kobzar", "Taras Shevchenko", "Kobzar"};

	sql << "select id, author, title from books where id=?;",
			from(expected.front()), into(strings);
	BOOST_CHECK_EQUAL_COLLECTIONS(strings.begin(), strings.end(),
								  expected.begin(), expected.end());

	strings.clear();
	expected = {"Kobzar", "Moby-Dick"};

	sql << "select title from books;", into(strings);
	BOOST_CHECK_EQUAL_COLLECTIONS(strings.begin(), strings.end(),
								  expected.begin(), expected.end());

	std::list<test::book> books;
	std::list<test::book> expected_books{
		{"kobzar", "Taras Shevchenko", "Kobzar", 452, 1.5, true},
		{"mdick", "Herman Melville", "Moby-Dick", 927, 3.2, false} };

	sql << "select * from books;", into(books);
	BOOST_CHECK_EQUAL(books.size(), expected_books.size());
	auto it = books.begin(); auto eit = expected_books.begin();
	for (; it != books.end(); ++it, ++eit)
		BOOST_CHECK(boost::fusion::equal_to(*it, *eit));

	// assure empty list
	books.clear();
	std::string non_existent("unknown");
	sql << "select * from books where id=?;", from(non_existent), into(books);
	BOOST_CHECK(books.empty());
}

