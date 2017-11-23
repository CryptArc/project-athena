//
//  AudioScope.h
//  interace/src/audio
//
//  Created by Stephen Birarda on 2014-12-16.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AudioScope_h
#define hifi_AudioScope_h

#include <glm/glm.hpp>

#include <QByteArray>
#include <QObject>

#include <DependencyManager.h>
#include <gpu/Batch.h>


class AudioScope : public QObject, public Dependency {
    Q_OBJECT
    SINGLETON_DEPENDENCY
    
    Q_PROPERTY(QVector<int> scopeInput READ getScopeInput)
    Q_PROPERTY(QVector<int> scopeOutputLeft READ getScopeOutputLeft)
    Q_PROPERTY(QVector<int> scopeOutputRight READ getScopeOutputRight)

    Q_PROPERTY(QVector<int> triggerInput READ getTriggerInput)
    Q_PROPERTY(QVector<int> triggerOutputLeft READ getTriggerOutputLeft)
    Q_PROPERTY(QVector<int> triggerOutputRight READ getTriggerOutputRight)

public:
    // Audio scope methods for allocation/deallocation
    void allocateScope();
    void freeScope();
    void reallocateScope(int frames);
    
public slots:
    void toggle() { setVisible(!_isEnabled); }
    void setVisible(bool visible);
    bool getVisible() const { return _isEnabled; }

    void togglePause() { _isPaused = !_isPaused; }
    void setPause(bool paused) { _isPaused = paused; }
    bool getPause() { return _isPaused; }

    void toggleTrigger() { _autoTrigger = !_autoTrigger; }
    bool getAutoTrigger() { return _autoTrigger; }
    void setAutoTrigger(bool autoTrigger) { 
        _isTriggered = false;
        _autoTrigger = autoTrigger; 
    }

    void setTriggered(bool triggered) { _isTriggered = triggered; }
    bool getTriggered() { return _isTriggered; }

    void selectAudioScopeFiveFrames();
    void selectAudioScopeTwentyFrames();
    void selectAudioScopeFiftyFrames();

    void setEnabled(bool enabled) { _isEnabled = enabled; }
    bool getEnabled() { return _isEnabled; }
    const int getAudioScopeBackground() { return _audioScopeBackground; }

    QVector<int> getScopeInput() { return _scopeInputData; };
    QVector<int> getScopeOutputLeft() { return _scopeOutputLeftData; };
    QVector<int> getScopeOutputRight() { return _scopeOutputRightData; };

    QVector<int> getTriggerInput() { return _triggerInputData; };
    QVector<int> getTriggerOutputLeft() { return _triggerOutputLeftData; };
    QVector<int> getTriggerOutputRight() { return _triggerOutputRightData; };

    void setTriggerValues(float x, float y);
    
protected:
    AudioScope();
    
private slots:
    void addStereoSilenceToScope(int silentSamplesPerChannel);
    void addLastFrameRepeatedWithFadeToScope(int samplesPerChannel);
    void addStereoSamplesToScope(const QByteArray& samples);
    void addInputToScope(const QByteArray& inputSamples);
    
private:
    
    // Audio scope methods for data acquisition
    int addBufferToScope(QByteArray* byteArray, int frameOffset, const int16_t* source, int sourceSamples,
                         unsigned int sourceChannel, unsigned int sourceNumberOfChannels, float fade = 1.0f);
    int addSilenceToScope(QByteArray* byteArray, int frameOffset, int silentSamples);
    
    QVector<int> getScopeVector(const QByteArray* scope, int offset);

    bool shouldTrigger(const QVector<int>& scope);
    void computeInputData();
    void computeOutputData();

    bool _isEnabled;
    bool _isPaused;
    bool _isTriggered;
    int _scopeInputOffset;
    int _scopeOutputOffset;
    int _framesPerScope;
    int _samplesPerScope;
    QByteArray* _scopeInput;
    QByteArray* _scopeOutputLeft;
    QByteArray* _scopeOutputRight;
    QByteArray _scopeLastFrame;
    
    QVector<int> _scopeInputData;
    QVector<int> _scopeOutputLeftData;
    QVector<int> _scopeOutputRightData;

    QVector<int> _triggerInputData;
    QVector<int> _triggerOutputLeftData;
    QVector<int> _triggerOutputRightData;

    bool _autoTrigger;
    glm::vec2 _triggerValues;

    int _audioScopeBackground;
    int _audioScopeGrid;
    int _inputID;
    int _outputLeftID;
    int _outputRightD;
};

#endif // hifi_AudioScope_h
