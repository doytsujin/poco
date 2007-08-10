//
// Session.h
//
// $Id: //poco/Main/Data/include/Poco/Data/Session.h#9 $
//
// Library: Data
// Package: DataCore
// Module:  Session
//
// Definition of the Session class.
//
// Copyright (c) 2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
// 
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//


#ifndef Data_Session_INCLUDED
#define Data_Session_INCLUDED


#include "Poco/Data/Data.h"
#include "Poco/Data/SessionImpl.h"
#include "Poco/Data/Statement.h"
#include "Poco/Data/StatementCreator.h"
#include "Poco/Data/Binding.h"
#include "Poco/AutoPtr.h"
#include "Poco/Any.h"


namespace Poco {
namespace Data {


class StatementImpl;


class Data_API Session
	/// A Session holds a connection to a Database and creates Statement objects.
	///
	/// Sessions are always created via the SessionFactory:
	///    
	///     Session ses(SessionFactory::instance().create(connectorKey, connectionString));
	///    
	/// where the first param presents the type of session one wants to create (e.g., for SQLite one would choose "SQLite",
	/// for ODBC the key is "ODBC") and the second param is the connection string that the session implementation 
	/// requires to connect to the database. The format of the connection string is specific to the actual connector.
	///
	/// A simpler form to create the session is to pass the connector key and connection string directly to
	/// the Session constructor.
	///
	/// A concrete example to open an SQLite database stored in the file "dummy.db" would be
	///    
	///     Session ses("SQLite", "dummy.db");
	///    
	/// Via a Session one can create two different types of statements. First, statements that should only be executed once and immediately, and
	/// second, statements that should be executed multiple times, using a separate execute() call.
	/// The simple one is immediate execution:
	///    
	///     ses << "CREATE TABLE Dummy (data INTEGER(10))", now;
	///
	/// The now at the end of the statement is required, otherwise the statement
	/// would not be executed.
	///    
	/// If one wants to reuse a Statement (and avoid the overhead of repeatedly parsing an SQL statement)
	/// one uses an explicit Statement object and its execute() method:
	///    
	///     int i = 0;
	///     Statement stmt = (ses << "INSERT INTO Dummy VALUES(:data)", use(i));
	///    
	///     for (i = 0; i < 100; ++i)
	///     {
	///         stmt.execute();
	///     }
	///    
	/// The above example assigns the variable i to the ":data" placeholder in the SQL query. The query is parsed and compiled exactly
	/// once, but executed 100 times. At the end the values 0 to 99 will be present in the Table "DUMMY".
	///
	/// A faster implementaton of the above code will simply create a vector of int
	/// and use the vector as parameter to the use clause (you could also use set or multiset instead):
	///    
	///     std::vector<int> data;
	///     for (int i = 0; i < 100; ++i)
	///     {
	///         data.push_back(i);
	///     }
	///     ses << "INSERT INTO Dummy VALUES(:data)", use(data);
	///
	/// NEVER try to bind to an empty collection. This will give a BindingException at run-time!
	///
	/// Retrieving data from a database works similar, you could use simple data types, vectors, sets or multiset as your targets:
	///
	///     std::set<int> retData;
	///     ses << "SELECT * FROM Dummy", into(retData));
	///
	/// Due to the blocking nature of the above call it is possible to partition the data retrieval into chunks by setting a limit to
	/// the maximum number of rows retrieved from the database:
	///
	///     std::set<int> retData;
	///     Statement stmt = (ses << "SELECT * FROM Dummy", into(retData), limit(50));
	///     while (!stmt.done())
	///     {
	///         stmt.execute();
	///     }
	///
	/// The "into" keyword is used to inform the statement where output results should be placed. The limit value ensures
	/// that during each run at most 50 rows are retrieved. Assuming Dummy contains 100 rows, retData will contain 50 
	/// elements after the first run and 100 after the second run, i.e.
	/// the collection is not cleared between consecutive runs. After the second execute stmt.done() will return true.
	///
	/// A prepared Statement will behave exactly the same but a further call to execute() will simply reset the Statement, 
	/// execute it again and append more data to the result set.
	///
	/// Note that it is possible to append several "bind" or "into" clauses to the statement. Theoretically, one could also have several
	/// limit clauses but only the last one that was added will be effective. 
	/// Also several preconditions must be met concerning binds and intos.
	/// Take the following example:
	///
	///     ses << "CREATE TABLE Person (LastName VARCHAR(30), FirstName VARCHAR, Age INTEGER(3))";
	///     std::vector<std::string> nameVec; // [...] add some elements
	///     std::vector<int> ageVec; // [...] add some elements
	///     ses << "INSERT INTO Person (LastName, Age) VALUES(:ln, :age)", use(nameVec), use(ageVec);
	///
	/// The size of all use parameters MUST be the same, otherwise an exception is thrown. Furthermore,
	/// the amount of use clauses must match the number of wildcards in the query (to be more precisely: 
	/// each binding has a numberOfColumnsHandled() value which is per default 1. The sum of all these values must match the wildcard count in the query.
	/// But this is only important if you have written your own TypeHandler specializations).
	/// If you plan to map complex object types to tables see the TypeHandler documentation.
	/// For now, we simply assume we have written one TypeHandler for Person objects. Instead of having n different vectors,
	/// we have one collection:
	///
	///     std::vector<Person> people; // [...] add some elements
	///     ses << "INSERT INTO Person (LastName, FirstName, Age) VALUES(:ln, :fn, :age)", use(people);
	///
	/// which will insert all Person objects from the people vector to the database (and again, you can use set, multiset too,
	/// even map and multimap if Person provides an operator() which returns the key for the map).
	/// The same works for a SELECT statement with "into" clauses:
	///
	///     std::vector<Person> people;
	///     ses << "SELECT * FROM PERSON", into(people);
{
public:
	Session(Poco::AutoPtr<SessionImpl> ptrImpl);
		/// Creates the Session.

	Session(const std::string& connector, const std::string& connectionString);
		/// Creates a new session, using the given connector (which must have
		/// been registered), and connectionString.

	Session(const Session&);
		/// Creates a session by copying another one.

	Session& operator = (const Session&);
		/// Assignement operator.

	~Session();
		/// Destroys the Session.

	void swap(Session& other);
		/// Swaps the session with another one.

	template <typename T>
	Statement operator << (const T& t)
		/// Creates a Statement with the given data as SQLContent
	{
		return _statementCreator << t;
	}

	StatementImpl* createStatementImpl();
		/// Creates a StatementImpl.

	void begin();
		/// Starts a transaction.

	void commit();
		/// Commits and ends a transaction.

	void rollback();
		/// Rolls back and ends a transaction.
	
	void close();
		/// Closes the session.

	bool isConnected();
		/// Returns true iff session is connected, false otherwise.

	bool isTransaction();
		/// Returns true iff a transaction is in progress, false otherwise.

	void setFeature(const std::string& name, bool state);
		/// Set the state of a feature.
		///
		/// Features are a generic extension mechanism for session implementations.
		/// and are defined by the underlying SessionImpl instance.
		///
		/// Throws a NotSupportedException if the requested feature is
		/// not supported by the underlying implementation.
	
	bool getFeature(const std::string& name) const;
		/// Look up the state of a feature.
		///
		/// Features are a generic extension mechanism for session implementations.
		/// and are defined by the underlying SessionImpl instance.
		///
		/// Throws a NotSupportedException if the requested feature is
		/// not supported by the underlying implementation.

	void setProperty(const std::string& name, const Poco::Any& value);
		/// Set the value of a property.
		///
		/// Properties are a generic extension mechanism for session implementations.
		/// and are defined by the underlying SessionImpl instance.
		///
		/// Throws a NotSupportedException if the requested property is
		/// not supported by the underlying implementation.

	Poco::Any getProperty(const std::string& name) const;
		/// Look up the value of a property.
		///
		/// Properties are a generic extension mechanism for session implementations.
		/// and are defined by the underlying SessionImpl instance.
		///
		/// Throws a NotSupportedException if the requested property is
		/// not supported by the underlying implementation.

	SessionImpl* impl();
		/// Returns a pointer to the underlying SessionImpl.

private:
	Session();

	Poco::AutoPtr<SessionImpl> _ptrImpl;
	StatementCreator           _statementCreator;
};


//
// inlines
//
inline StatementImpl* Session::createStatementImpl()
{
	return _ptrImpl->createStatementImpl();
}


inline void Session::begin()
{
	return _ptrImpl->begin();
}


inline void Session::commit()
{
	return _ptrImpl->commit();
}


inline void Session::rollback()
{
	return _ptrImpl->rollback();
}


inline void Session::close()
{
	_ptrImpl->close();
}


inline bool Session::isConnected()
{
	return _ptrImpl->isConnected();
}


inline bool Session::isTransaction()
{
	return _ptrImpl->isTransaction();
}


inline void Session::setFeature(const std::string& name, bool state)
{
	_ptrImpl->setFeature(name, state);
}


inline bool Session::getFeature(const std::string& name) const
{
	return const_cast<SessionImpl*>(_ptrImpl.get())->getFeature(name);
}


inline void Session::setProperty(const std::string& name, const Poco::Any& value)
{
	_ptrImpl->setProperty(name, value);
}


inline Poco::Any Session::getProperty(const std::string& name) const
{
	return const_cast<SessionImpl*>(_ptrImpl.get())->getProperty(name);
}


inline SessionImpl* Session::impl()
{
	return _ptrImpl;
}


} } // namespace Poco::Data


#endif // Data_Session_INCLUDED
