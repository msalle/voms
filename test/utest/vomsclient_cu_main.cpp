
#include <cppunit/TestResult.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TextOutputter.h>
#include <cppunit/XmlOutputter.h>

#include <iostream>
#include <fstream>

#include <vomsclient_cu_suite.h>

int main()
{
  std::ofstream xml("../reports/vomsclient_cu.xml", std::ios::app);

  CppUnit::TestResult controller;
  CppUnit::TestResultCollector result;

  controller.addListener(&result);
  CppUnit::TestRunner runner;

  runner.addTest(vomsclient_test::suite());
  runner.run(controller);

  CppUnit::XmlOutputter outputter(&result, xml);
  CppUnit::TextOutputter outputter2(&result, std::cerr);
  outputter.write();
  outputter2.write();

  return (result.wasSuccessful() ? 0 : 1);
}