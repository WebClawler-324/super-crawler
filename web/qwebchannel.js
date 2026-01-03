/*!
 * QWebChannel: A library for seamless integration between Qt and HTML/JavaScript.
 *
 * Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 * Copyright (C) 2014 Lionel Chauvin <megabigbug@yahoo.fr>
 *
 * This file is part of the QtWebChannel module of the Qt Toolkit.
 *
 * $QT_BEGIN_LICENSE:LGPL$
 * Commercial License Usage
 * Licensees holding valid commercial Qt licenses may use this file in
 * accordance with the commercial license agreement provided with the
 * Software or, alternatively, in accordance with the terms contained in
 * a written agreement between you and Digia.  For licensing terms and
 * conditions see http://qt.digia.com/licensing.  For further information
 * use the contact form at http://qt.digia.com/contact-us.
 *
 * GNU Lesser General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU Lesser
 * General Public License version 2.1 as published by the Free Software
 * Foundation and appearing in the file LICENSE.LGPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU Lesser General Public License version 2.1 requirements
 * will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 *
 * In addition, as a special exception, Digia gives you certain additional
 * rights.  These rights are described in the Digia Qt LGPL Exception
 * version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl.html.
 *
 * $QT_END_LICENSE$
 */

(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports) :
    typeof define === 'function' && define.amd ? define(['exports'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, factory(global.QWebChannel = {}));
})(this, (function (exports) { 'use strict';

    /**
     * @class QWebChannel
     * @constructor
     * @param {Object} transport - The transport object used to communicate with the Qt application.
     * @param {Function} [initCallback] - A callback function that is called when the channel is initialized.
     * @description Creates a new QWebChannel instance that communicates with the Qt application via the given transport.
     */
    function QWebChannel(transport, initCallback) {
        if (typeof transport !== 'object' || typeof transport.send !== 'function') {
            throw new Error('The transport object must have a send function.');
        }

        this.transport = transport;
        this.send = function (data) {
            transport.send(JSON.stringify(data));
        };

        this.objects = {};
        this.callbacks = {};
        this.callbackId = 0;

        var self = this;
        transport.onmessage = function (event) {
            var data = JSON.parse(event.data);
            self.handleMessage(data);
        };

        if (initCallback) {
            this.initCallback = initCallback;
        }

        this.send({ type: 'init' });
    }

    QWebChannel.prototype = {
        /**
         * @method handleMessage
         * @param {Object} message - The message received from the Qt application.
         * @description Handles incoming messages from the Qt application.
         */
        handleMessage: function (message) {
            switch (message.type) {
                case 'init':
                    this.init(message.data);
                    break;
                case 'signal':
                    this.handleSignal(message.data);
                    break;
                case 'response':
                    this.handleResponse(message.data);
                    break;
                case 'error':
                    console.error('QWebChannel error:', message.data);
                    break;
                default:
                    console.warn('Unknown QWebChannel message type:', message.type);
            }
        },

        /**
         * @method init
         * @param {Object} data - The initialization data containing object descriptions.
         * @description Initializes the QWebChannel with the given object descriptions.
         */
        init: function (data) {
            var self = this;
            Object.keys(data).forEach(function (id) {
                self.objects[id] = new QObject(data[id], self);
            });

            if (this.initCallback) {
                this.initCallback(this.objects);
            }
        },

        /**
         * @method handleSignal
         * @param {Object} data - The signal data containing object id, signal name, and arguments.
         * @description Handles incoming signals from Qt objects.
         */
        handleSignal: function (data) {
            var object = this.objects[data.object];
            if (!object) {
                console.warn('Received signal for unknown object:', data.object);
                return;
            }

            object.emit(data.signal, data.args);
        },

        /**
         * @method handleResponse
         * @param {Object} data - The response data containing callback id and return value.
         * @description Handles responses to method calls made to Qt objects.
         */
        handleResponse: function (data) {
            var callback = this.callbacks[data.id];
            if (!callback) {
                console.warn('Received response for unknown callback:', data.id);
                return;
            }

            if (data.error) {
                callback.error(data.error);
            } else {
                callback.apply(null, data.args);
            }

            delete this.callbacks[data.id];
        },

        /**
         * @method invokeMethod
         * @param {QObject} object - The Qt object to invoke the method on.
         * @param {string} method - The name of the method to invoke.
         * @param {Array} args - The arguments to pass to the method.
         * @param {Function} [callback] - The callback function to call with the result.
         * @description Invokes a method on a Qt object and calls the callback with the result.
         */
        invokeMethod: function (object, method, args, callback) {
            var id = this.callbackId++;
            this.callbacks[id] = callback || function () {};

            this.send({
                type: 'invoke',
                data: {
                    id: id,
                    object: object.id,
                    method: method,
                    args: this.serializeArguments(args)
                }
            });
        },

        /**
         * @method serializeArguments
         * @param {Array} args - The arguments to serialize.
         * @returns {Array} The serialized arguments.
         * @description Serializes arguments for sending to the Qt application.
         */
        serializeArguments: function (args) {
            var serialized = [];
            for (var i = 0; i < args.length; i++) {
                var arg = args[i];
                if (arg instanceof QObject) {
                    serialized.push({ type: 'qobject', id: arg.id });
                } else if (Array.isArray(arg)) {
                    serialized.push({ type: 'array', value: this.serializeArguments(arg) });
                } else if (typeof arg === 'object' && arg !== null) {
                    serialized.push({ type: 'object', value: this.serializeArguments(Object.values(arg)) });
                } else {
                    serialized.push({ type: 'value', value: arg });
                }
            }
            return serialized;
        }
    };

    /**
     * @class QObject
     * @constructor
     * @param {Object} description - The description of the Qt object.
     * @param {QWebChannel} channel - The QWebChannel instance associated with this object.
     * @description Creates a new QObject instance representing a Qt object.
     */
    function QObject(description, channel) {
        this.id = description.id;
        this.channel = channel;
        this.properties = description.properties || {};
        this.signals = description.signals || [];
        this.methods = description.methods || [];

        var self = this;

        // Create getters for properties
        Object.keys(this.properties).forEach(function (name) {
            Object.defineProperty(self, name, {
                get: function () {
                    return self.properties[name];
                },
                set: function (value) {
                    self.properties[name] = value;
                    self.channel.send({
                        type: 'setProperty',
                        data: {
                            object: self.id,
                            name: name,
                            value: self.channel.serializeArguments([value])[0]
                        }
                    });
                },
                enumerable: true
            });
        });

        // Create signal handlers
        this.signalHandlers = {};
        this.signals.forEach(function (name) {
            self.signalHandlers[name] = [];
            self[name] = function (handler) {
                if (typeof handler === 'function') {
                    self.signalHandlers[name].push(handler);
                }
            };
        });

        // Create methods
        this.methods.forEach(function (name) {
            self[name] = function () {
                var args = Array.prototype.slice.call(arguments);
                var callback = typeof args[args.length - 1] === 'function' ? args.pop() : null;
                self.channel.invokeMethod(self, name, args, callback);
            };
        });
    }

    QObject.prototype = {
        /**
         * @method emit
         * @param {string} signal - The name of the signal to emit.
         * @param {Array} args - The arguments to pass to the signal handlers.
         * @description Emits a signal and calls all registered handlers.
         */
        emit: function (signal, args) {
            var handlers = this.signalHandlers[signal];
            if (!handlers) {
                return;
            }

            for (var i = 0; i < handlers.length; i++) {
                handlers[i].apply(this, args);
            }
        }
    };

    // Export QWebChannel and QObject to the global scope
    exports.QWebChannel = QWebChannel;
    exports.QObject = QObject;

}));