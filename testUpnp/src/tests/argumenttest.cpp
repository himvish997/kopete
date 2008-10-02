
#include "argumenttest.h"
#include "argument.h"
#include <QString>

ArgumentTest::ArgumentTest()
{
}

ArgumentTest::~ArgumentTest()
{
}

void ArgumentTest::testArgument() 
{
	Argument arg;
	QVERIFY(arg.name().isNull());
	QVERIFY(arg.direction().isNull());
	QVERIFY(arg.relatedStateVariable().isNull());
}

void ArgumentTest::testSetName()
{
	Argument arg;
	arg.setName(QString("name_arg"));
	QCOMPARE(arg.name(),QString("name_arg"));
}

void ArgumentTest::testSetDirection()
{
	Argument arg;
	arg.setDirection(QString("my_direction"));
	QCOMPARE(arg.direction(),QString("my_direction"));
}

void ArgumentTest::testSetRelatedStateVariable()
{
	Argument arg;
	arg.setRelatedStateVariable(QString("state_variable"));
	QCOMPARE(arg.relatedStateVariable(),QString("state_variable"));
}

void ArgumentTest::testOperatorEquals()
{
	Argument argument, argument2, argument3;
	argument.setName(QString("name_test"));
	argument.setDirection(QString("direction_test"));
	argument.setRelatedStateVariable(QString("state_variable_test"));
	argument2 = argument;
	QVERIFY(argument2 == argument);
	argument2.setName(QString("name_test2"));
	QVERIFY((argument2 == argument)==false);
	argument3.setName(QString("name_test2"));
	argument3.setDirection(QString("direction_test"));
	argument3.setRelatedStateVariable(QString("state_variable_test"));
	QVERIFY((argument == argument3)==false);
	QVERIFY(argument2 == argument3);	
}

QTEST_KDEMAIN_CORE(ArgumentTest)

