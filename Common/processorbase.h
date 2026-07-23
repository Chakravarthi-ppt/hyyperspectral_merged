#ifndef PROCESSORBASE_H
#define PROCESSORBASE_H

#include <QObject>
#include <QString>

class ProcessorBase : public QObject
{
    Q_OBJECT

public:

    explicit ProcessorBase(QObject *parent =nullptr)
        : QObject(parent)
    {
    }

    QString workingFolder() const
    {
        return mWorkingFolder;
    }

    QString preProcessedFolder() const
    {
        return mPreProcessedFolder;
    }

protected:

    QString mWorkingFolder;

    QString mPreProcessedFolder;

signals:

    void progressChanged(
            int value,
            const QString &message);

    void processingFinished(
            const QString &folder);
};


#endif // PROCESSORBASE_H
