// MAVLink Inspector - Vanilla JavaScript with WebSocket

class MAVLinkInspector {
    constructor() {
        // Message storage
        this.messages = new Map();
        this.selectedMessage = null;
        this.showRaw = false;
        
        // Chart
        this.chart = null;
        this.chartData = new Map();
        
        // WebSocket connection
        this.socket = null;
        
        // UI elements
        this.elements = {
            statusIndicator: document.getElementById('statusIndicator'),
            statusText: document.getElementById('statusText'),
            messageList: document.getElementById('messageList'),
            messageDetail: document.getElementById('messageDetail'),
            detailTitle: document.getElementById('detailTitle'),
            searchInput: document.getElementById('searchInput'),
            clearBtn: document.getElementById('clearBtn'),
            rawBtn: document.getElementById('rawBtn'),
            chartPanel: document.getElementById('chartPanel'),
            fieldCheckboxes: document.getElementById('fieldCheckboxes'),
            timeRangeSelect: document.getElementById('timeRangeSelect'),
            messageChart: document.getElementById('messageChart')
        };
        
        // Selected fields for chart
        this.selectedFields = new Set();
        
        this.initEventListeners();
        this.initChart();
        this.connect();
    }
    
    initEventListeners() {
        this.elements.searchInput.addEventListener('input', (e) => {
            this.filterMessages(e.target.value);
        });
        
        this.elements.clearBtn.addEventListener('click', () => {
            this.clearMessages();
        });
        
        this.elements.rawBtn.addEventListener('click', () => {
            this.toggleRawView();
        });
        
        this.elements.timeRangeSelect.addEventListener('change', () => {
            this.updateChart();
        });
    }
    
    initChart() {
        const ctx = this.elements.messageChart.getContext('2d');
        this.chart = new Chart(ctx, {
            type: 'line',
            data: {
                datasets: []
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            unit: 'second'
                        },
                        grid: {
                            color: '#3e3e3e'
                        },
                        ticks: {
                            color: '#888888'
                        },
                        display: false
                    },
                    y: {
                        grid: {
                            color: '#3e3e3e'
                        },
                        ticks: {
                            color: '#888888'
                        }
                    }
                },
                plugins: {
                    legend: {
                        labels: {
                            color: '#cccccc'
                        }
                    }
                },
                elements: {
                    line: {
                        borderWidth: 1.5
                    },
                    point: {
                        radius: 0,
                        hitRadius: 5
                    }
                }
            }
        });
    }
    
    connect() {
        this.updateStatus('connecting', 'Connecting...');
        
        // Connect to Socket.IO server
        this.socket = io({
            transports: ['websocket', 'polling']
        });
        
        // Connection events
        this.socket.on('connect', () => {
            console.log('WebSocket connected');
            this.updateStatus('connected', 'Connected');
        });
        
        this.socket.on('disconnect', () => {
            console.log('WebSocket disconnected');
            this.updateStatus('disconnected', 'Disconnected');
        });
        
        this.socket.on('connect_error', (error) => {
            console.error('Connection error:', error);
            this.updateStatus('disconnected', 'Connection failed');
        });
        
        // Status updates from server
        this.socket.on('status', (data) => {
            this.updateStatus(data.state, data.text);
        });
        
        // Message list updates
        this.socket.on('message_list', (messages) => {
            for (const msg of messages) {
                if (!this.messages.has(msg.name)) {
                    this.messages.set(msg.name, {
                        count: msg.count,
                        frequency: msg.frequency,
                        lastData: {}
                    });
                } else {
                    const localMsg = this.messages.get(msg.name);
                    localMsg.count = msg.count;
                    localMsg.frequency = msg.frequency;
                }
            }
            this.renderMessageList();
        });
        
        // Individual message updates
        this.socket.on('message', (data) => {
            this.handleMessage(data.name, data.data);
        });
        
        // Message detail response
        this.socket.on('message_detail', (detail) => {
            if (detail.name === this.selectedMessage) {
                const msg = this.messages.get(detail.name);
                if (msg) {
                    msg.lastData = detail.data;
                    
                    // Update chart data from server's field history
                    if (detail.fieldHistory) {
                        for (const [field, history] of Object.entries(detail.fieldHistory)) {
                            const fieldKey = `${detail.name}.${field}`;
                            this.chartData.set(fieldKey, history);
                        }
                    }
                    
                    this.renderMessageDetail();
                }
            }
        });
    }
    
    handleMessage(messageName, data) {
        if (!this.messages.has(messageName)) {
            this.messages.set(messageName, {
                count: 0,
                frequency: 0,
                lastData: null
            });
        }
        
        const msg = this.messages.get(messageName);
        msg.lastData = data;
        
        // Update chart data if this message is selected
        if (this.selectedMessage === messageName) {
            this.updateChartData(messageName, data);
            this.renderMessageDetail();
            
            // Update chart if any fields are selected
            if (this.selectedFields.size > 0) {
                this.updateChart();
            }
        }
    }
    
    updateChartData(messageName, data) {
        const now = Date.now();
        
        // Store numeric field values for charting
        for (const [key, value] of Object.entries(data)) {
            if (typeof value === 'number') {
                const fieldKey = `${messageName}.${key}`;
                
                if (!this.chartData.has(fieldKey)) {
                    this.chartData.set(fieldKey, []);
                }
                
                const dataPoints = this.chartData.get(fieldKey);
                dataPoints.push({ timestamp: now, value });
                
                // Keep limited history
                const maxAge = 600000; // 10 minutes
                this.chartData.set(fieldKey, dataPoints.filter(p => now - p.timestamp < maxAge));
            }
        }
    }
    
    renderMessageList() {
        const searchTerm = this.elements.searchInput.value.toLowerCase();
        const sortedMessages = Array.from(this.messages.entries())
            .filter(([name]) => name.toLowerCase().includes(searchTerm))
            .sort((a, b) => b[1].count - a[1].count);
        
        this.elements.messageList.innerHTML = sortedMessages.map(([name, msg]) => `
            <div class="message-item ${this.selectedMessage === name ? 'active' : ''}" 
                 data-message="${name}">
                <div class="message-name">${name}</div>
                <div class="message-meta">
                    <span class="message-count">${msg.count} msgs</span>
                    <span class="message-frequency">${msg.frequency.toFixed(1)} Hz</span>
                </div>
            </div>
        `).join('');
        
        // Add click handlers
        document.querySelectorAll('.message-item').forEach(item => {
            item.addEventListener('click', () => {
                this.selectMessage(item.dataset.message);
            });
        });
    }
    
    selectMessage(messageName) {
        this.selectedMessage = messageName;
        this.renderMessageList();
        
        // Request detailed data from server
        if (this.socket && this.socket.connected) {
            this.socket.emit('request_detail', { message: messageName });
        }
        
        this.updateFieldCheckboxes();
    }
    
    renderMessageDetail() {
        if (!this.selectedMessage) {
            this.elements.messageDetail.innerHTML = '<p class="placeholder">Click on a message type to view details</p>';
            return;
        }
        
        const msg = this.messages.get(this.selectedMessage);
        this.elements.detailTitle.textContent = this.selectedMessage;
        
        if (this.showRaw) {
            this.elements.messageDetail.innerHTML = `
                <pre class="raw-content">${JSON.stringify(msg.lastData, null, 2)}</pre>
            `;
        } else {
            const fields = Object.entries(msg.lastData).map(([key, value]) => `
                <tr>
                    <td class="field-name">${key}</td>
                    <td class="field-value">${this.formatValue(value)}</td>
                </tr>
            `).join('');
            
            this.elements.messageDetail.innerHTML = `
                <table class="field-table">
                    <thead>
                        <tr>
                            <th>Field</th>
                            <th>Value</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${fields}
                    </tbody>
                </table>
            `;
        }
    }
    
    formatValue(value) {
        if (Array.isArray(value)) {
            return `[${value.join(', ')}]`;
        }
        if (typeof value === 'number') {
            if (Number.isInteger(value)) {
                return value.toString();
            }
            return value.toFixed(6);
        }
        return String(value);
    }
    
    updateFieldCheckboxes() {
        if (!this.selectedMessage) {
            this.elements.fieldCheckboxes.innerHTML = '<span style="color: #888888; font-size: 13px;">Select a message to view fields</span>';
            this.elements.chartPanel.style.display = 'none';
            return;
        }
        
        const msg = this.messages.get(this.selectedMessage);
        const numericFields = Object.entries(msg.lastData)
            .filter(([_, value]) => typeof value === 'number')
            .map(([key]) => key);
        
        if (numericFields.length === 0) {
            this.elements.fieldCheckboxes.innerHTML = '<span style="color: #888888; font-size: 13px;">No numeric fields available</span>';
            this.elements.chartPanel.style.display = 'none';
            return;
        }
        
        // Show chart panel when message with numeric fields is selected
        this.elements.chartPanel.style.display = 'flex';
        
        this.elements.fieldCheckboxes.innerHTML = numericFields.map(field => {
            const isChecked = this.selectedFields.has(field);
            return `
                <div class="field-checkbox">
                    <input type="checkbox" id="field_${field}" ${isChecked ? 'checked' : ''} data-field="${field}">
                    <label for="field_${field}">${field}</label>
                </div>
            `;
        }).join('');
        
        // Add event listeners to checkboxes
        this.elements.fieldCheckboxes.querySelectorAll('input[type="checkbox"]').forEach(checkbox => {
            checkbox.addEventListener('change', (e) => {
                const field = e.target.dataset.field;
                if (e.target.checked) {
                    this.selectedFields.add(field);
                } else {
                    this.selectedFields.delete(field);
                }
                this.updateChart();
            });
        });
    }
    

    
    toggleRawView() {
        this.showRaw = !this.showRaw;
        this.renderMessageDetail();
    }
    
    updateChart() {
        if (!this.selectedMessage || this.selectedFields.size === 0) {
            this.chart.data.datasets = [];
            this.chart.update();
            return;
        }
        
        const timeRange = parseInt(this.elements.timeRangeSelect.value) * 1000;
        const now = Date.now();
        
        // Color palette for multiple fields
        const colors = [
            '#007acc', '#4ec9b0', '#dcdcaa', '#f48771', '#c586c0',
            '#9cdcfe', '#ce9178', '#6a9955', '#569cd6', '#d16969'
        ];
        
        // Create dataset for each selected field
        const datasets = Array.from(this.selectedFields).map((field, index) => {
            const fieldKey = `${this.selectedMessage}.${field}`;
            const data = this.chartData.get(fieldKey) || [];
            const filteredData = data.filter(p => now - p.timestamp < timeRange);
            
            const color = colors[index % colors.length];
            
            return {
                label: field,
                data: filteredData.map(p => ({
                    x: p.timestamp,
                    y: p.value
                })),
                borderColor: color,
                backgroundColor: color + '20',
                tension: 0.3
            };
        });
        
        this.chart.data.datasets = datasets;
        this.chart.update('none'); // 'none' mode for better performance
    }
    
    filterMessages(searchTerm) {
        this.renderMessageList();
    }
    
    clearMessages() {
        this.messages.clear();
        this.chartData.clear();
        this.selectedMessage = null;
        this.selectedFields.clear();
        this.renderMessageList();
        this.renderMessageDetail();
        this.updateFieldCheckboxes();
        this.updateChart();
        
        // Notify server to clear
        if (this.socket && this.socket.connected) {
            this.socket.emit('clear');
        }
    }
    
    updateStatus(state, text) {
        this.elements.statusIndicator.className = `status-dot ${state}`;
        this.elements.statusText.textContent = text;
    }
}

// Initialize the application when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    new MAVLinkInspector();
});
