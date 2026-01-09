clear; clc;

%% Set port 
% List available serial ports (Windows)
ports = serialportlist("available");

if isempty(ports)
    error("No serial ports detected. Check USB cable, drivers, and device connection.");
end

disp("Available ports:");
disp(ports);

% Select port
% Option 1: Automatically use the first detected COM port
port = ports(1);

% Option 2 (uncomment if you want a specific port, e.g. COM3)
% port = "COM3";

disp("Using port: " + port);

% Configure serial connection
baud = 115200;
s = serialport(port, baud, "Timeout", 5);

configureTerminator(s, "CR/LF");  % expects \r\n
flush(s);                         % clear buffer


%% Initialise visualisation plots 
figure('Name','Temperature Sensor Monitor','NumberTitle','off');
tiledlayout(2,2);

ax(1) = nexttile;
title(ax(1),'MAX31855 (Thermocouple)');
xlabel(ax(1),'Time (s)'); ylabel(ax(1),'Temperature (°C)');
h(1) = animatedline(ax(1));

ax(2) = nexttile;
title(ax(2),'TMP36 (Analog)');
xlabel(ax(2),'Time (s)'); ylabel(ax(2),'Temperature (°C)');
h(2) = animatedline(ax(2));

ax(3) = nexttile;
title(ax(3),'PT1000 (RTD)');
xlabel(ax(3),'Time (s)'); ylabel(ax(3),'Temperature (°C)');
h(3) = animatedline(ax(3));

ax(4) = nexttile;
title(ax(4),'MLX90614 (IR)');
xlabel(ax(4),'Time (s)'); ylabel(ax(4),'Temperature (°C)');
h(4) = animatedline(ax(4));

for k = 1:4
    grid(ax(k),'on');
end

%% define range 
Y_RANGE = 2;   % degrees C

for k = 1:4
    grid(ax(k),'on');
    ax(k).YLim = [0 Y_RANGE];   % 0–20°C initial window for ALL axes
end

%% Initialise data output (CSV)
logFilename = ['tsensor_log_' datestr(now,'yyyymmdd_HHMMSS') '.csv'];
% logHeader = {'Time (s)', 'Thermocouple', 'Analog', 'RTD', 'IR'};

% Write to file 
fid = fopen(logFilename, 'w');
if fid == -1
    error('Could not create log file: %s', logFilename);
end
fprintf(fid, 'Time_s,Thermocouple,Analog,RTD,IR\n');
fclose(fid);

lastLoggedSec = -1;   % 1-second steps

%% Generate live visualisation 
% maxRunTime = 3000;  % seconds

cleanupObj = onCleanup(@()generate_results(h, s)); % clean-up register

disp('Reading data...');
tic;

lastIR = NaN; 

% Data acquisition loop
while true
    try
        line = readline(s);  

        % Initialise command window data 
        disp("RAW: " + strtrim(line)); 
        vals_int = sscanf(line, '%d,%d,%d,%d');
        if numel(vals_int) ~= 4
            warning("Malformed line, skipping");
            continue;
        end
        
        % Temperature conversion 
        vals = double(vals_int) / 100;   

        % IR filtering (do NOT block plotting)
        if vals(4) == 0
            vals(4) = NaN;
        else
            lastIR = vals(4);
        end

        % Print all data values 
        fprintf('T1=%.2f  T2=%.2f  T3=%.2f  T4=%.2f\n', ...
                vals(1), vals(2), vals(3), vals(4));
        t = toc;

        %% Loop live visualisation
        RECENTER_THRESHOLD = 1.0;  % degrees C: recenter if change > 1°C

        for i = 1:4
            addpoints(h(i), t, vals(i));
            ax(i).XLim = [max(0, t-60), t+1]; % 60-second window range

            y = vals(i);
            if ~isnan(y)
                yMin = ax(i).YLim(1);
                yMax = ax(i).YLim(2);
                currentCenter = (yMin + yMax) / 2;

                % If we haven't set proper limits yet, or span is wrong
                if (yMax - yMin) ~= Y_RANGE || isinf(currentCenter)
                    currentCenter = y;
                    yMin = currentCenter - Y_RANGE/2;
                    yMax = currentCenter + Y_RANGE/2;
                else
                    % Only recenter when reading moves by > threshold
                    if abs(y - currentCenter) > RECENTER_THRESHOLD
                        currentCenter = y;
                        yMin = currentCenter - Y_RANGE/2;
                        yMax = currentCenter + Y_RANGE/2;
                    end
                end

                ax(i).YLim = [yMin, yMax];
            end
        end
        drawnow limitrate;
        %% Loop data log 
        currentSec = floor(t);
        if currentSec > lastLoggedSec
            lastLoggedSec = currentSec;

            fid = fopen(logFilename, 'a');
            if fid ~= -1
                fprintf(fid, '%.3f,%.2f,%.2f,%.2f,%.2f\n', ...
                        t, vals(1), vals(2), vals(3), vals(4));
                fclose(fid);
            else
                warning('Could not open log file for appending: %s', logFilename);
            end
        end

    catch ME
        warning("Error reading/parsing line: %s", ME.message);
    end
end

%% Generate static outputs (.png, .csv)
function generate_results(h, s)

    fprintf('\nStopping logging...\n');

    if isempty(h) || any(~isvalid(h))
        warning('Data invalid: no data generated.');
    else
        % Get data 
        [t1, y1] = getpoints(h(1));
        [t2, y2] = getpoints(h(2));
        [t3, y3] = getpoints(h(3));
        [t4, y4] = getpoints(h(4));

        % Create static figure
        fig = figure('Name','Complete Temperature Profiles (All Sensors)', ...
                     'NumberTitle','off');
        tl = tiledlayout(2,2);

        names = { ...
            'Thermocouple', ...
            'Analog Sensor', ...
            'RTD', ...
            'IR Sensor'};

        nexttile;
        plot(t1, y1, 'LineWidth', 1.2);
        grid on;
        title(names{1});
        xlabel('Time (s)');
        ylabel('Temperature (°C)');

        nexttile;
        plot(t2, y2, 'LineWidth', 1.2);
        grid on;
        title(names{2});
        xlabel('Time (s)');
        ylabel('Temperature (°C)');

        nexttile;
        plot(t3, y3, 'LineWidth', 1.2);
        grid on;
        title(names{3});
        xlabel('Time (s)');
        ylabel('Temperature (°C)');

        nexttile;
        plot(t4, y4, 'LineWidth', 1.2);
        grid on;
        title(names{4});
        xlabel('Time (s)');
        ylabel('Temperature (°C)');

        title(tl, 'Complete Temperature Profiles (All Sensors)');

        % Save static plot to file
        pngName = ['tsensor_static_' datestr(now,'yyyymmdd_HHMMSS') '.png'];
        saveas(fig, pngName);
        savefig(fig, strrep(pngName, '.png', '.fig'));

        % Static single 
        figSingle = figure('Name','Temperature Profiles (All Sensors)', ...
                            'NumberTitle','off');
        hold on;
        plot(t1, y1, 'LineWidth', 1.2);
        plot(t2, y2, 'LineWidth', 1.2);
        plot(t3, y3, 'LineWidth', 1.2);
        plot(t4, y4, 'LineWidth', 1.2);
        grid on;
        xlabel('Time (s)');
        ylabel('Temperature (°C)');
        title('Temperature Profiles (All Sensors)');
        legend(names, 'Location', 'best');

        allT = [t1(:); t2(:); t3(:); t4(:)];
        if ~isempty(allT)
            xlim([max(0, min(allT)) max(allT)]);
        end

        singlePng = ['Temperature Profiles (All Sensors)' datestr(now,'yyyymmdd_HHMMSS') '.png'];
        saveas(figSingle, singlePng);
        savefig(figSingle, strrep(singlePng, '.png', '.fig'));
    end

    % Serial cleanup
    if ~isempty(s) && isvalid(s)
        clear s;
    end
end
