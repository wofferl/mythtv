/*
 *  Class TestEITFixups
 *
 *  Copyright (C) David Hampton 2018
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include "mythiowrapper.h"
#include "test_mythiowrapper.h"

using namespace std;

void TestMythIOWrapper::initTestCase(void)
{
}

void TestMythIOWrapper::local_directory_test(void)
{
    QSet<QString> known = QSet<QString> { "foo", "baz", "bar" };

    QString dirname = QString(QT_TESTCASE_BUILDDIR) + "/testfiles";
    int dirid = mythdir_opendir(qPrintable(dirname));
    QVERIFY2(dirid != 0, "mythdir_opendir failed");

    char *name;
    QSet<QString> found;
    while ((name = mythdir_readdir(dirid)) != NULL) {
        if (name[0] != '.')
            found += name;
        free(name);
    }
    QVERIFY(known == found);

    int res =  mythdir_closedir(dirid);
    QVERIFY2(res == 0, "mythdir_closedir failed");
}

void TestMythIOWrapper::cleanupTestCase(void)
{
}

QTEST_APPLESS_MAIN(TestMythIOWrapper)
