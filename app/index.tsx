import React, { useState, useEffect, useRef } from 'react';
import {
  View, Text, StyleSheet, TouchableOpacity,
  ScrollView, Alert, Vibration
} from 'react-native';
import Paho from 'paho-mqtt';

const MQTT_HOST  = 'ee6cc7485d49430d8cf8a9101fd3b475.s1.eu.hivemq.cloud';
const MQTT_PORT  = 8884;
const MQTT_USER  = 'pico_sensor';
const MQTT_PASS  = 'Golfaina679?';
const MQTT_TOPIC = 'iot/sensors';
const MQTT_ALERT = 'iot/alerts';

export default function App() {
  const [connected,  setConnected]  = useState(false);
  const [lastSeen,   setLastSeen]   = useState(null);
  const [temp,       setTemp]       = useState(null);
  const [hum,        setHum]        = useState(null);
  const [flame1,     setFlame1]     = useState(false);
  const [flame2,     setFlame2]     = useState(false);
  const [flame1Raw,  setFlame1Raw]  = useState(0);
  const [flame2Raw,  setFlame2Raw]  = useState(0);
  const [history,    setHistory]    = useState([]);
  const [page,       setPage]       = useState('dashboard');
  const [fireAlert,  setFireAlert]  = useState(false);
  const clientRef = useRef(null);

  useEffect(() => {
    const client = new Paho.Client(
      MQTT_HOST,
      Number(MQTT_PORT),
      '/mqtt',
      'rn_' + Math.random().toString(16).slice(2)
    );

    client.onConnectionLost = () => {
      console.log('MQTT disconnected');
      setConnected(false);
    };

    client.onMessageArrived = (message) => {
      const topic = message.destinationName;
      const raw   = message.payloadString;
      console.log('MSG:', topic, raw);
      try {
        const data = JSON.parse(raw);
        if (topic === MQTT_TOPIC) {
          setTemp(data.t);
          setHum(data.h);
          setFlame1(data.fs1 === 1);
          setFlame2(data.fs2 === 1);
          setFlame1Raw(data.f1);
          setFlame2Raw(data.f2);
          setLastSeen(new Date().toLocaleTimeString());
          setHistory(prev => [{
            time: new Date().toLocaleTimeString(),
            temp: data.t,
            hum:  data.h,
          }, ...prev].slice(0, 20));
        }
        if (topic === MQTT_ALERT && data.alert === 'FIRE') {
          setFireAlert(true);
          Vibration.vibrate([0, 500, 200, 500, 200, 500]);
          Alert.alert(
            '🔥 FIRE DETECTED!',
            `Sensor 1: ${data.s1 ? 'ACTIVE' : 'OK'}\nSensor 2: ${data.s2 ? 'ACTIVE' : 'OK'}`,
            [{ text: 'Acknowledge', onPress: () => setFireAlert(false) }]
          );
        }
      } catch (e) {
        console.log('Parse error:', e);
      }
    };

    client.connect({
      useSSL: true,
      userName: MQTT_USER,
      password: MQTT_PASS,
      onSuccess: () => {
        console.log('MQTT connected!');
        setConnected(true);
        client.subscribe(MQTT_TOPIC);
        client.subscribe(MQTT_ALERT);
      },
      onFailure: (err) => {
        console.log('MQTT failed:', err.errorMessage);
        setConnected(false);
      }
    });

    clientRef.current = client;
    return () => {
      if (client.isConnected()) client.disconnect();
    };
  }, []);

  const Dashboard = () => (
    <ScrollView style={styles.page}>
      {fireAlert && (
        <TouchableOpacity
          style={styles.fireBanner}
          onPress={() => setFireAlert(false)}
        >
          <Text style={styles.fireBannerText}>🔥 FIRE DETECTED — TAP TO DISMISS</Text>
        </TouchableOpacity>
      )}

      <View style={[styles.card, { borderColor: '#ff6b35' }]}>
        <Text style={styles.cardIcon}>🌡️</Text>
        <Text style={styles.cardLabel}>TEMPERATURE</Text>
        <Text style={styles.cardValue}>
          {temp !== null ? `${temp.toFixed(1)}°C` : '--.-°C'}
        </Text>
        <Text style={styles.cardSub}>
          {temp !== null
            ? temp > 30 ? '🔴 High'
            : temp > 20 ? '🟡 Normal'
            : '🔵 Cool'
            : 'Waiting for data...'}
        </Text>
      </View>

      <View style={[styles.card, { borderColor: '#4fc3f7' }]}>
        <Text style={styles.cardIcon}>💧</Text>
        <Text style={styles.cardLabel}>HUMIDITY</Text>
        <Text style={styles.cardValue}>
          {hum !== null ? `${hum.toFixed(1)}%` : '--.-%'}
        </Text>
        <Text style={styles.cardSub}>
          {hum !== null
            ? hum > 70 ? '🔴 Very Humid'
            : hum > 40 ? '🟡 Normal'
            : '🔵 Dry'
            : 'Waiting for data...'}
        </Text>
      </View>

      <View style={[styles.card, {
        borderColor: (flame1 || flame2) ? '#ff1744' : '#69f0ae'
      }]}>
        <Text style={styles.cardIcon}>{(flame1 || flame2) ? '🔥' : '✅'}</Text>
        <Text style={styles.cardLabel}>FLAME SENSORS</Text>
        <Text style={[styles.cardValue, {
          color: (flame1 || flame2) ? '#ff1744' : '#69f0ae',
          fontSize: 36,
        }]}>
          {(flame1 || flame2) ? 'FIRE!' : 'SAFE'}
        </Text>
        <View style={styles.barRow}>
          <Text style={styles.barLabel}>S1</Text>
          <View style={styles.barBg}>
            <View style={[styles.barFill, {
              width: `${(flame1Raw / 4095) * 100}%`,
              backgroundColor: flame1 ? '#ff1744' : '#69f0ae',
            }]} />
          </View>
          <Text style={styles.barValue}>{flame1Raw}</Text>
        </View>
        <View style={styles.barRow}>
          <Text style={styles.barLabel}>S2</Text>
          <View style={styles.barBg}>
            <View style={[styles.barFill, {
              width: `${(flame2Raw / 4095) * 100}%`,
              backgroundColor: flame2 ? '#ff1744' : '#69f0ae',
            }]} />
          </View>
          <Text style={styles.barValue}>{flame2Raw}</Text>
        </View>
      </View>

      <Text style={styles.lastSeen}>
        Last update: {lastSeen || 'Waiting for data...'}
      </Text>
    </ScrollView>
  );

  const History = () => (
    <ScrollView style={styles.page}>
      <Text style={styles.historyTitle}>Sensor History</Text>
      {history.length === 0 ? (
        <Text style={styles.noData}>No data yet...</Text>
      ) : (
        history.map((item, i) => (
          <View key={i} style={styles.historyRow}>
            <Text style={styles.historyTime}>{item.time}</Text>
            <Text style={styles.historyTemp}>🌡 {item.temp?.toFixed(1)}°C</Text>
            <Text style={styles.historyHum}>💧 {item.hum?.toFixed(1)}%</Text>
          </View>
        ))
      )}
    </ScrollView>
  );

  return (
    <View style={styles.container}>

      {/* STATUS BAR */}
      <View style={styles.statusBar}>
        <Text style={styles.statusText}>IoT Sensor Node</Text>
        <View style={[styles.dot, {
          backgroundColor: connected ? '#00ff88' : '#ff4444'
        }]} />
        <Text style={styles.statusSmall}>
          {connected ? 'Live' : 'Offline'}
        </Text>
      </View>

      {/* CONTENT */}
      {page === 'dashboard' ? <Dashboard /> : <History />}

      {/* NAV BAR */}
      <View style={styles.navBar}>
        <TouchableOpacity
          style={[styles.navBtn, page === 'dashboard' && styles.navBtnActive]}
          onPress={() => setPage('dashboard')}
        >
          <Text style={styles.navIcon}>📊</Text>
          <Text style={styles.navLabel}>Dashboard</Text>
        </TouchableOpacity>
        <TouchableOpacity
          style={[styles.navBtn, page === 'history' && styles.navBtnActive]}
          onPress={() => setPage('history')}
        >
          <Text style={styles.navIcon}>📋</Text>
          <Text style={styles.navLabel}>History</Text>
        </TouchableOpacity>
      </View>

    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0a0a0a',
  },
  statusBar: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1a1a1a',
    paddingTop: 50,
    paddingBottom: 12,
    paddingHorizontal: 20,
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  statusText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
    flex: 1,
  },
  statusSmall: {
    color: '#aaa',
    fontSize: 12,
    marginLeft: 6,
  },
  dot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
  page: {
    flex: 1,
    padding: 16,
  },
  card: {
    backgroundColor: '#1a1a1a',
    borderRadius: 16,
    borderWidth: 2,
    padding: 20,
    marginBottom: 16,
    alignItems: 'center',
  },
  cardIcon: {
    fontSize: 40,
    marginBottom: 8,
  },
  cardLabel: {
    color: '#aaa',
    fontSize: 12,
    letterSpacing: 2,
    marginBottom: 8,
  },
  cardValue: {
    color: '#fff',
    fontSize: 48,
    fontWeight: 'bold',
    marginBottom: 8,
  },
  cardSub: {
    color: '#aaa',
    fontSize: 14,
  },
  barRow: {
    flexDirection: 'row',
    alignItems: 'center',
    width: '100%',
    marginTop: 10,
  },
  barLabel: {
    color: '#aaa',
    width: 25,
    fontSize: 12,
  },
  barBg: {
    flex: 1,
    height: 8,
    backgroundColor: '#333',
    borderRadius: 4,
    marginHorizontal: 8,
    overflow: 'hidden',
  },
  barFill: {
    height: '100%',
    borderRadius: 4,
  },
  barValue: {
    color: '#aaa',
    width: 45,
    fontSize: 11,
    textAlign: 'right',
  },
  lastSeen: {
    color: '#555',
    fontSize: 12,
    textAlign: 'center',
    marginBottom: 20,
  },
  fireBanner: {
    backgroundColor: '#ff1744',
    padding: 14,
    borderRadius: 10,
    marginBottom: 16,
    alignItems: 'center',
  },
  fireBannerText: {
    color: '#fff',
    fontWeight: 'bold',
    fontSize: 14,
  },
  historyTitle: {
    color: '#fff',
    fontSize: 20,
    fontWeight: 'bold',
    marginBottom: 16,
  },
  historyRow: {
    flexDirection: 'row',
    backgroundColor: '#1a1a1a',
    borderRadius: 10,
    padding: 12,
    marginBottom: 8,
    alignItems: 'center',
  },
  historyTime: {
    color: '#aaa',
    fontSize: 12,
    width: 80,
  },
  historyTemp: {
    color: '#ff6b35',
    fontSize: 13,
    flex: 1,
  },
  historyHum: {
    color: '#4fc3f7',
    fontSize: 13,
    flex: 1,
  },
  noData: {
    color: '#555',
    textAlign: 'center',
    marginTop: 40,
    fontSize: 16,
  },
  navBar: {
    flexDirection: 'row',
    backgroundColor: '#1a1a1a',
    borderTopWidth: 1,
    borderTopColor: '#333',
    paddingBottom: 20,
    paddingTop: 10,
  },
  navBtn: {
    flex: 1,
    alignItems: 'center',
    paddingVertical: 4,
    opacity: 0.5,
  },
  navBtnActive: {
    opacity: 1,
  },
  navIcon: {
    fontSize: 22,
  },
  navLabel: {
    color: '#fff',
    fontSize: 11,
    marginTop: 2,
  },
});
