Search

- [Qt 6.11](https://doc.qt.io/qt-6/index.html)
- [All Overviews](https://doc.qt.io/qt-6/overviews.html)
- Debugging Techniques

On this page

[Testing and Debugging](https://doc.qt.io/qt-6/testing-and-debugging.html) [Deploying Qt Applications](https://doc.qt.io/qt-6/deployment.html)

# Debugging Techniques

Here we present some useful hints to help you with debugging your Qt-based software.

## Configuring Qt for Debugging [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#configuring-qt-for-debugging "Direct link to this headline")

When [configuring](https://doc.qt.io/qt-6/configure-options.html) Qt for installation, it is possible to ensure that it is built to include debug symbols that can make it easier to track bugs in applications and libraries. However, on some platforms, building Qt in debug mode will cause applications to be larger than desirable.

### Debugging in macOS and Xcode [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#debugging-in-macos-and-xcode "Direct link to this headline")

#### Debugging With/Without Frameworks [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#debugging-with-without-frameworks "Direct link to this headline")

The basic stuff you need to know about debug libraries and frameworks is found at developer.apple.com in: [Apple Technical Note TN2124](https://developer.apple.com/library/archive/technotes/tn2124/_index.html).

When you build Qt, frameworks are built by default, and inside the framework you will find both a release and a debug version (e.g., [QtCore](https://doc.qt.io/qt-6/qtcore-module.html) and QtCore\_debug). If you pass the `-no-framework` flag when you build Qt, two dylibs are built for each Qt library (e.g., libQtCore.4.dylib and libQtCore\_debug.4.dylib).

What happens when you link depends on whether you use frameworks or not. We don't see a compelling reason to recommend one over the other.

##### With Frameworks: [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#with-frameworks "Direct link to this headline")

Since the release and debug libraries are inside the framework, the app is simply linked against the framework. Then when you run in the debugger, you will get either the release version or the debug version, depending on whether you set `DYLD_IMAGE_SUFFIX`. If you don't set it, you get the release version by default (i.e., non \_debug). If you set `DYLD_IMAGE_SUFFIX=_debug`, you get the debug version.

##### Without Frameworks: [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#without-frameworks "Direct link to this headline")

When you tell _qmake_ to generate a Makefile with the debug config, it will link against the \_debug version of the libraries and generate debug symbols for the app. Running this program in GDB will then work like running GDB on other platforms, and you will be able to trace inside Qt.

## Command Line Options Recognized by Qt [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#command-line-options-recognized-by-qt "Direct link to this headline")

When you run a Qt application, you can specify several command-line options that can help with debugging. These are recognized by [QApplication](https://doc.qt.io/qt-6/qapplication.html).

| Option | Description |
| --- | --- |
| `-nograb` | The application should never grab [the mouse](https://doc.qt.io/qt-6/qwidget.html#grabMouse) or [the keyboard](https://doc.qt.io/qt-6/qwidget.html#grabKeyboard). This option is set by default when the program is running in the `gdb` debugger under Linux. |
| `-dograb` | Ignore any implicit or explicit `-nograb`. `-dograb` wins over `-nograb` even when `-nograb` is last on the command line. |

## Environment Variables Recognized by Qt [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#environment-variables-recognized-by-qt "Direct link to this headline")

At runtime, a Qt application recognizes many environment variables, some of which can be helpful for debugging:

| Variable | Description |
| --- | --- |
| `QT_DEBUG_PLUGINS` | Set to a non-zero value to make Qt print out diagnostic information about the each (C++) plugin it tries to load. |
| `QML_IMPORT_TRACE` | Set to a non-zero value to make QML print out diagnostic information from the import loading mechanism. |
| `QT_HASH_SEED` | Set to an integer value to disable [QHash](https://doc.qt.io/qt-6/qhash.html#qhash) and [QSet](https://doc.qt.io/qt-6/qset.html) using a new random ordering for each application run, which in some cases might make testing and debugging difficult. |
| `QT_WIN_DEBUG_CONSOLE` | On Windows, GUI applications are not attached to a console, so output written to `stdout` and `stderr` is not visible to the user. IDEs typically redirect and display output, but when running an application from the command line, debug output is lost. To get access to the output, set this environment variable to `new` to make the application allocate a new console, or to `attach` to make the application attempt to attach to the parent process' console. |

## Warning and Debugging Messages [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#warning-and-debugging-messages "Direct link to this headline")

Qt includes global C++ macros for writing out warning and debug text. The plain macros use a default [logging category](https://doc.qt.io/qt-6/qloggingcategory.html); the categorized logging macros allow you to specify the category. You can use them for the following purposes:

| Plain macro | Categorized macro | Purpose |
| --- | --- | --- |
| [qDebug](https://doc.qt.io/qt-6/qdebug.html#qDebug)() | [qCDebug](https://doc.qt.io/qt-6/qloggingcategory.html#qCDebug)() | Used for writing custom debug output |
| [qInfo](https://doc.qt.io/qt-6/qdebug.html#qInfo)() | [qCInfo](https://doc.qt.io/qt-6/qloggingcategory.html#qCInfo)() | Used for informational messages |
| [qWarning](https://doc.qt.io/qt-6/qdebug.html#qWarning)() | [qCWarning](https://doc.qt.io/qt-6/qloggingcategory.html#qCWarning)() | Used to report warnings and recoverable errors in your application or library |
| [qCritical](https://doc.qt.io/qt-6/qdebug.html#qCritical)() | [qCCritical](https://doc.qt.io/qt-6/qloggingcategory.html#qCCritical)() | Used for writing critical error messages and reporting system errors |
| [qFatal](https://doc.qt.io/qt-6/qdebug.html#qFatal)() | - | Used for writing messages about fatal errors shortly before exiting |

If you include the <QtDebug> header file, the `qDebug()` macro can also be used as an output stream. For example:

```
qDebug() << "Widget" << widget << "at position" << widget->pos();
```

The Qt implementation of these macros prints to the `stderr` output under Unix/Linux and macOS. With Windows, if it is a console application, the text is sent to console; otherwise, it is sent to the debugger.

By default, only the message is printed. You can include additional information by setting the `QT_MESSAGE_PATTERN` environment variable. For example:

```
QT_MESSAGE_PATTERN="[%{time process} %{type}] %{appname} %{category} %{function} - %{message}"
```

The format is documented in [qSetMessagePattern](https://doc.qt.io/qt-6/qtlogging.html#qSetMessagePattern)(). You can also install your own message handler using [qInstallMessageHandler](https://doc.qt.io/qt-6/qtlogging.html#qInstallMessageHandler)().

If the `QT_FATAL_WARNINGS` environment variable is set, [qWarning](https://doc.qt.io/qt-6/qdebug.html#qWarning)() exits after printing the warning message. This makes it easy to obtain a backtrace in the debugger.

[qDebug](https://doc.qt.io/qt-6/qdebug.html#qDebug)(), [qInfo](https://doc.qt.io/qt-6/qdebug.html#qInfo)(), and [qWarning](https://doc.qt.io/qt-6/qdebug.html#qWarning)() are debugging tools. They can be compiled away by defining `QT_NO_DEBUG_OUTPUT`, `QT_NO_INFO_OUTPUT`, or `QT_NO_WARNING_OUTPUT` during compilation.

The debugging functions [QObject::dumpObjectTree](https://doc.qt.io/qt-6/qobject.html#dumpObjectTree)() and [QObject::dumpObjectInfo](https://doc.qt.io/qt-6/qobject.html#dumpObjectInfo)() are often useful when an application looks or acts strangely. More useful if you use [object names](https://doc.qt.io/qt-6/qobject.html#setObjectName) than not, but often useful even without names.

In QML, [dumpItemTree](https://doc.qt.io/qt-6/qml-qtquick-item.html#dumpItemTree-method)() serves the same purpose.

## Providing Support for the qDebug() Stream Operator [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#providing-support-for-the-qdebug-stream-operator "Direct link to this headline")

You can implement the stream operator used by [qDebug](https://doc.qt.io/qt-6/qdebug.html#qDebug)() to provide debugging support for your classes. The class that implements the stream is `QDebug`. Use `QDebugStateSaver` to temporarily save the formatting options of the stream. Use [nospace](https://doc.qt.io/qt-6/qdebug.html#nospace)() and [QTextStream manipulators](https://doc.qt.io/qt-6/qtextstream.html#qtextstream-manipulators) to further customize the formatting.

Here is an example for a class that represents a 2D coordinate.

```
QDebug operator<<(QDebug dbg, const Coordinate &c)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "(" << c.x() << ", " << c.y() << ")";

    return dbg;
}
```

Integration of custom types with Qt's meta-object system is covered in more depth in the [Creating Custom Qt Types](https://doc.qt.io/qt-6/custom-types.html) document.

## Debugging Macros [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#debugging-macros "Direct link to this headline")

The header file `<QtGlobal>` contains some debugging macros and `#define`s.

Three important macros are:

- [Q\_ASSERT](https://doc.qt.io/qt-6/qtassert.html#Q_ASSERT)(cond), where `cond` is a boolean expression, writes the warning "ASSERT: ' _cond_' in file xyz.cpp, line 234" and exits if `cond` is false.
- [Q\_ASSERT\_X](https://doc.qt.io/qt-6/qtassert.html#Q_ASSERT_X)(cond, where, what), where `cond` is a boolean expression, `where` a location, and `what` a message, writes the warning: "ASSERT failure in `where`: '`what`', file xyz.cpp, line 234" and exits if `cond` is false.
- [Q\_CHECK\_PTR](https://doc.qt.io/qt-6/qtassert.html#Q_CHECK_PTR)(ptr), where `ptr` is a pointer. Writes the warning "In file xyz.cpp, line 234: Out of memory" and exits if `ptr` is 0.

These macros are useful for detecting program errors, e.g. like this:

```
char *alloc(int size)
{
    Q_ASSERT(size > 0);
    char *ptr = new char[size];
    Q_CHECK_PTR(ptr);
    return ptr;
}
```

[Q\_ASSERT](https://doc.qt.io/qt-6/qtassert.html#Q_ASSERT)(), [Q\_ASSERT\_X](https://doc.qt.io/qt-6/qtassert.html#Q_ASSERT_X)(), and [Q\_CHECK\_PTR](https://doc.qt.io/qt-6/qtassert.html#Q_CHECK_PTR)() expand to nothing if `QT_NO_DEBUG` is defined during compilation. For this reason, the arguments to these macro should not have any side-effects. Here is an incorrect usage of [Q\_CHECK\_PTR](https://doc.qt.io/qt-6/qtassert.html#Q_CHECK_PTR)():

```
char *alloc(int size)
{
    char *ptr;
    Q_CHECK_PTR(ptr = new char[size]);  // WRONG
    return ptr;
}
```

If this code is compiled with `QT_NO_DEBUG` defined, the code in the [Q\_CHECK\_PTR](https://doc.qt.io/qt-6/qtassert.html#Q_CHECK_PTR)() expression is not executed and _alloc_ returns an uninitialized pointer.

The Qt library contains hundreds of internal checks that will print warning messages when a programming error is detected. We therefore recommend that you use a debug version of Qt when developing Qt-based software.

Logging and [categorized logging](https://doc.qt.io/qt-6/qml-qtqml-loggingcategory.html) are also possible in [QML](https://doc.qt.io/qt-6/qtquick-debugging.html).

## Common Bugs [Direct link to this headline](https://doc.qt.io/qt-6/debug.html\#common-bugs "Direct link to this headline")

There is one bug that is so common that it deserves mention here: If you include the [Q\_OBJECT](https://doc.qt.io/qt-6/qobject.html#Q_OBJECT) macro in a class declaration and run [the meta-object compiler](https://doc.qt.io/qt-6/moc.html) (`moc`), but forget to link the `moc`-generated object code into your executable, you will get very confusing error messages. Any link error complaining about a lack of `vtbl`, `_vtbl`, `__vtbl` or similar is likely to be a result of this problem.

[Testing and Debugging](https://doc.qt.io/qt-6/testing-and-debugging.html) [Deploying Qt Applications](https://doc.qt.io/qt-6/deployment.html)

© 2026 The Qt Company Ltd.
Documentation contributions included herein are the copyrights of
their respective owners. The documentation provided herein is licensed under the terms of the [GNU Free Documentation License version 1.3](http://www.gnu.org/licenses/fdl.html) as published by the Free Software Foundation. Qt and respective logos are [trademarks](https://doc.qt.io/qt/trademarks.html) of The Qt Company Ltd. in Finland and/or other countries
worldwide. All other trademarks are property of their respective owners.

SurveyMonkey Powered Online Survey

### Question Title

#### \*    Was this page helpful?

Yes



No



### Question Title

#### \*    What could be better in Qt documentation?

Add or update content/information



Improve the explanation/tutorials



Additional examples/code snippets



Clarify links/API references



Improve structure/navigation



Other suggestion (please specify)




Submit



[(](https://www.surveymonkey.com/mp/legal/privacy/?ut_source=survey_pp&white_label=1) Privacy & Cookie Notice