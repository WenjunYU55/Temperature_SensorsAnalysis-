clear; close all; clc;

% Read data
gtFile = "tsensor_groundtruth.csv"; 
rawFile = "tsensor_raw.csv";
rtFile = "responsetime.csv";
stFile = "stability.csv";

gt = readtable(gtFile, "VariableNamingRule","preserve");
raw = readtable(rawFile, "VariableNamingRule","preserve");
rt = readtable(rtFile, "VariableNamingRule","preserve");
st = readtable(stFile, "VariableNamingRule","preserve");

% Groundtruth: [time, measurement]
t_gt = gt{:,1};
y_gt = gt{:,2};

% Raw Sensors: [time, "Thermocouple", "Analogue", "RTD", "IR"]
t_raw = raw{:,1};
Y_raw = raw{:,2:end};
sensorNames = string(raw.Properties.VariableNames(2:end));

% Response Time
t_rt = rt{:,1};
Y_rt = rt{:,2:end};
names_rt = string(rt.Properties.VariableNames(2:end));

% Stability 
t_st = st{:,1};
Y_st = st{:,2:end};
names_st = string(st.Properties.VariableNames(2:end));

% Plot Raw
sensorColor = containers.Map( ...
    lower(["analogue","thermocouple","rtd","ir"]), ...
    {hex2rgb("#4a86e8"), hex2rgb("#fbbc04"), hex2rgb("#34a853"), hex2rgb("#ea4335")} );
getColor = @(nm) sensorColor(lower(string(nm)));

for i = 1:size(Y_raw,2)
    figure('Name',"Sensor vs Ground Truth: " + sensorNames(i),'Color','w');
    hold on;

    c = getColor(sensorNames(i));
    scatter(t_raw, Y_raw(:,i), 12, 'filled', ...
        'MarkerFaceColor', c, ...
        'MarkerEdgeColor', 'none');

    scatter(t_gt, y_gt, 30, 'filled', ...
        'MarkerFaceColor',[0 0 0], ...
        'MarkerEdgeColor','none', ...
        'MarkerFaceAlpha',0.2);

    grid on;
    xlabel('Time (s)');
    ylabel('Temperature (C)');
    title("Experimental Sensor Measurement vs Ground Truth: " + sensorNames(i));
    legend([sensorNames(i), "Ground Truth"], 'Location','best');
end

% Plot Response Time
figure('Name',"Response Time",'Color','w');
sp1 = stackedplot(t_rt, Y_rt, 'DisplayLabels', names_rt);
title(sp1, "Response Time");
xlabel(sp1, "Time (s)");
% ylabel(sp1, "Temperature (C)");
grid on;
drawnow;
for k = 1:numel(sp1.LineProperties)
    sp1.LineProperties(k).LineWidth = 1.5;
end

% Plot Drift
figure('Name',"Stability/Drift",'Color','w');
sp2 = stackedplot(t_st, Y_st, 'DisplayLabels', names_st);
title(sp2, "Time Drift");
xlabel(sp2, "Time (s)");
% ylabel(sp2, "Temperature (C)");
grid on;
drawnow; 
for k = 1:numel(sp2.AxesProperties)
    sp2.AxesProperties(k).YLimits = [20 40];
end
for k = 1:numel(sp2.LineProperties)
    sp2.LineProperties(k).LineWidth = 1.5;
end


function rgb = hex2rgb(hex)
    hex = char(hex);
    if hex(1) == '#'
        hex = hex(2:end); 
    end
    rgb = [ ...
        hex2dec(hex(1:2)), ...
        hex2dec(hex(3:4)), ...
        hex2dec(hex(5:6)) ] / 255;
end