import React, { useState, useEffect, useRef } from 'react';
import { Button } from '@/components/ui/button';
import { Textarea } from '@/components/ui/textarea';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { ScrollArea } from '@/components/ui/scroll-area';
import { Badge } from '@/components/ui/badge';
import { Progress } from '@/components/ui/progress';
import { toast } from 'sonner';
import { Toaster } from '@/components/ui/sonner';
import {
  Bot, Play, Brain, Target, Search, Calculator, CheckCircle,
  AlertCircle, Settings, Copy, Trash2
} from 'lucide-react';

interface AgentTask {
  id: string;
  goal: string;
  status: 'idle' | 'planning' | 'executing' | 'completed' | 'failed';
  progress: number;
  thoughts: string[];
  actions: Array<{
    type: string;
    description: string;
    result: string;
    timestamp: string;
  }>;
  result?: string;
  createdAt: string;
}

const OllamaAgentSystem = () => {
  const [tasks, setTasks] = useState<AgentTask[]>([]);
  const [inputGoal, setInputGoal] = useState('');
  const [activeTaskId, setActiveTaskId] = useState<string | null>(null);
  const [isRunning, setIsRunning] = useState(false);
  const [ollamaHost, setOllamaHost] = useState('http://localhost:11434');
  const [model, setModel] = useState('llama3.2:3b');

  // Ollama API Integration
  const callOllama = async (prompt: string, system?: string): Promise<string> => {
    try {
      const response = await fetch(`${ollamaHost}/api/generate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          model: model,
          prompt: prompt,
          system: system || "You are a helpful AI assistant that can research, analyze, and provide detailed responses.",
          stream: false,
          options: {
            temperature: 0.7,
            top_k: 40,
            top_p: 0.9
          }
        })
      });

      if (!response.ok) {
        throw new Error(`Ollama API error: ${response.status}`);
      }

      const data = await response.json();
      return data.response || '';
    } catch (error) {
      console.error('Ollama API call failed:', error);
      throw new Error(`Failed to connect to Ollama: ${error}`);
    }
  };

  // Agent Tools
  const searchTool = async (query: string): Promise<string> => {
    const prompt = `Research and provide comprehensive information about: ${query}

Please provide:
1. Key facts and current information
2. Important statistics or data points
3. Recent developments or trends
4. Multiple perspectives if applicable
5. Reliable sources or references

Format your response as a detailed research summary.`;

    return await callOllama(prompt, "You are a research specialist. Provide comprehensive, factual information with proper analysis.");
  };

  const analyzeTool = async (data: string, context: string): Promise<string> => {
    const prompt = `Analyze the following information in the context of: ${context}

Data to analyze:
${data}

Please provide:
1. Key insights and patterns
2. Important conclusions
3. Implications and significance
4. Recommendations based on analysis
5. Areas requiring further investigation`;

    return await callOllama(prompt, "You are a data analyst. Provide thorough analysis with clear insights and actionable conclusions.");
  };

  const planTool = async (goal: string): Promise<string> => {
    const prompt = `Create a detailed execution plan for this goal: ${goal}

Provide:
1. Step-by-step breakdown of tasks
2. Required information or research needed
3. Logical sequence of actions
4. Success criteria
5. Potential challenges and solutions

Format as a clear, actionable plan.`;

    return await callOllama(prompt, "You are a strategic planner. Create comprehensive, practical plans that lead to goal achievement.");
  };

  const reasonTool = async (information: string, goal: string): Promise<string> => {
    const prompt = `Based on all the information gathered, provide reasoning and conclusions for: ${goal}

Available information:
${information}

Please provide:
1. Logical synthesis of all information
2. Clear conclusions and insights
3. Recommendations and next steps
4. Confidence level in conclusions
5. Final answer or solution`;

    return await callOllama(prompt, "You are a reasoning specialist. Synthesize information logically and provide clear, well-reasoned conclusions.");
  };

  // Agent Execution Engine
  const executeAgent = async (task: AgentTask) => {
    const updateTask = (updates: Partial<AgentTask>) => {
      setTasks(prev => prev.map(t => t.id === task.id ? { ...t, ...updates } : t));
    };

    try {
      setIsRunning(true);
      updateTask({ status: 'planning', progress: 10 });

      // Step 1: Planning
      const planResult = await planTool(task.goal);
      updateTask({
        actions: [...task.actions, {
          type: 'planning',
          description: 'Created execution plan',
          result: planResult,
          timestamp: new Date().toISOString()
        }],
        thoughts: [...task.thoughts, "Created detailed execution plan"],
        progress: 25
      });

      updateTask({ status: 'executing' });

      // Step 2: Research/Information Gathering
      const searchResult = await searchTool(task.goal);
      updateTask({
        actions: [...task.actions, {
          type: 'research',
          description: 'Gathered relevant information',
          result: searchResult,
          timestamp: new Date().toISOString()
        }],
        thoughts: [...task.thoughts, "Completed information gathering phase"],
        progress: 50
      });

      // Step 3: Analysis
      const analysisResult = await analyzeTool(searchResult, task.goal);
      updateTask({
        actions: [...task.actions, {
          type: 'analysis',
          description: 'Analyzed collected information',
          result: analysisResult,
          timestamp: new Date().toISOString()
        }],
        thoughts: [...task.thoughts, "Performed comprehensive analysis"],
        progress: 75
      });

      // Step 4: Final Reasoning and Conclusion
      const combinedInfo = `Plan: ${planResult}\n\nResearch: ${searchResult}\n\nAnalysis: ${analysisResult}`;
      const finalResult = await reasonTool(combinedInfo, task.goal);

      updateTask({
        status: 'completed',
        progress: 100,
        result: finalResult,
        actions: [...task.actions, {
          type: 'conclusion',
          description: 'Generated final conclusions',
          result: finalResult,
          timestamp: new Date().toISOString()
        }],
        thoughts: [...task.thoughts, "Task completed successfully"]
      });

      toast.success('Agent task completed!', { duration: 5000 });

    } catch (error) {
      console.error('Agent execution failed:', error);
      updateTask({
        status: 'failed',
        thoughts: [...task.thoughts, `Error: ${error}`]
      });
      toast.error('Agent task failed', {
        description: error instanceof Error ? error.message : 'Unknown error',
        duration: 5000
      });
    } finally {
      setIsRunning(false);
    }
  };

  const deployAgent = async () => {
    if (!inputGoal.trim() || isRunning) return;

    const task: AgentTask = {
      id: Date.now().toString(),
      goal: inputGoal,
      status: 'idle',
      progress: 0,
      thoughts: [],
      actions: [],
      createdAt: new Date().toISOString()
    };

    setTasks(prev => [task, ...prev]);
    setActiveTaskId(task.id);
    setInputGoal('');

    toast.success('Agent deployed!', { description: 'Starting autonomous execution' });

    // Start execution
    setTimeout(() => executeAgent(task), 1000);
  };

  const deleteTask = (taskId: string) => {
    setTasks(prev => prev.filter(t => t.id !== taskId));
    if (activeTaskId === taskId) setActiveTaskId(null);
    toast.success('Task deleted');
  };

  const copyResult = (result: string) => {
    navigator.clipboard.writeText(result);
    toast.success('Result copied to clipboard');
  };

  // Test Ollama Connection
  const testConnection = async () => {
    try {
      toast.info('Testing Ollama connection...');
      await callOllama('Hello, are you working?');
      toast.success('Ollama connection successful!');
    } catch (error) {
      toast.error('Ollama connection failed', {
        description: 'Make sure Ollama is running and the model is available'
      });
    }
  };

  const activeTask = tasks.find(t => t.id === activeTaskId);

  return (
    <div className="h-screen flex bg-black text-green-400 font-mono">
      <Toaster position="top-right" theme="dark" />

      {/* Tasks Sidebar */}
      <div className="w-80 border-r border-green-600/30 bg-gray-950">
        <Card className="h-full bg-transparent border-none">
          <CardHeader className="pb-3">
            <CardTitle className="text-green-400 text-sm flex items-center gap-2">
              <Bot className="w-4 h-4" />
              OLLAMA AGENTS
            </CardTitle>
          </CardHeader>
          <CardContent className="p-0">
            <ScrollArea className="h-[calc(100vh-100px)]">
              <div className="px-4 space-y-3">
                {tasks.length === 0 ? (
                  <div className="text-center py-12 text-gray-500">
                    <Brain className="w-12 h-12 mx-auto mb-3" />
                    <p className="text-sm">No Agents Deployed</p>
                  </div>
                ) : (
                  tasks.map((task) => (
                    <Card
                      key={task.id}
                      className={`cursor-pointer transition-all ${
                        activeTaskId === task.id ? 'bg-green-600/10 border-green-600/50' : 'bg-gray-900/50 border-gray-700/50'
                      }`}
                      onClick={() => setActiveTaskId(task.id)}
                    >
                      <CardContent className="p-3">
                        <div className="flex items-start justify-between mb-2">
                          <div className="flex-1 min-w-0">
                            <h3 className="text-sm font-medium text-green-400 truncate">
                              {task.goal}
                            </h3>
                            <Badge className={`h-5 text-xs mt-1 ${
                              task.status === 'completed' ? 'bg-green-600' :
                              task.status === 'executing' || task.status === 'planning' ? 'bg-yellow-600' :
                              task.status === 'failed' ? 'bg-red-600' : 'bg-gray-600'
                            }`}>
                              {task.status.toUpperCase()}
                            </Badge>
                          </div>
                          <Button
                            variant="ghost"
                            size="sm"
                            className="h-6 w-6 p-0 hover:bg-red-600/20"
                            onClick={(e) => {
                              e.stopPropagation();
                              deleteTask(task.id);
                            }}
                          >
                            <Trash2 className="w-3 h-3" />
                          </Button>
                        </div>
                        <Progress value={task.progress} className="h-2 bg-gray-800" />
                        {task.result && (
                          <Button
                            size="sm"
                            variant="outline"
                            onClick={(e) => {
                              e.stopPropagation();
                              copyResult(task.result!);
                            }}
                            className="w-full h-6 text-xs border-green-600 text-green-400 mt-2"
                          >
                            <Copy className="w-3 h-3 mr-1" />
                            Copy Result
                          </Button>
                        )}
                      </CardContent>
                    </Card>
                  ))
                )}
              </div>
            </ScrollArea>
          </CardContent>
        </Card>
      </div>

      {/* Main Area */}
      <div className="flex-1 flex flex-col">
        {/* Header */}
        <div className="border-b border-green-600/30 p-4 bg-gray-950">
          <div className="flex items-center justify-between">
            <div className="flex items-center space-x-3">
              <h1 className="text-green-400 font-bold text-lg">OLLAMA AGENT SYSTEM</h1>
              <Badge variant="outline" className={`${
                isRunning ? 'border-yellow-500 text-yellow-400' : 'border-green-500 text-green-400'
              }`}>
                <div className={`w-2 h-2 rounded-full mr-2 ${
                  isRunning ? 'bg-yellow-500 animate-pulse' : 'bg-green-500'
                }`}></div>
                {isRunning ? 'WORKING' : 'READY'}
              </Badge>
            </div>
            <div className="text-sm text-gray-400">
              Model: {model}
            </div>
          </div>
        </div>

        {/* Content Area */}
        <div className="flex-1 overflow-hidden">
          <ScrollArea className="h-full">
            <div className="p-4">
              {activeTask ? (
                <div className="space-y-4">
                  {/* Task Overview */}
                  <Card className="bg-gray-900/50 border-gray-700/50">
                    <CardHeader>
                      <CardTitle className="text-green-400 flex items-center gap-2">
                        <Target className="w-5 h-5" />
                        {activeTask.goal}
                      </CardTitle>
                    </CardHeader>
                    <CardContent>
                      <Progress value={activeTask.progress} className="h-3 bg-gray-800" />
                      <p className="text-xs text-gray-400 mt-2">
                        Progress: {activeTask.progress}% | Status: {activeTask.status}
                      </p>
                    </CardContent>
                  </Card>

                  {/* Agent Actions */}
                  {activeTask.actions.map((action, index) => (
                    <Card key={index} className="bg-gray-900/50 border-gray-700/50">
                      <CardContent className="p-4">
                        <div className="flex items-start gap-3">
                          <div className="p-2 rounded-full bg-green-600/20">
                            {action.type === 'planning' && <Target className="w-4 h-4" />}
                            {action.type === 'research' && <Search className="w-4 h-4" />}
                            {action.type === 'analysis' && <Brain className="w-4 h-4" />}
                            {action.type === 'conclusion' && <CheckCircle className="w-4 h-4" />}
                          </div>
                          <div className="flex-1">
                            <h4 className="text-green-400 font-medium mb-2">
                              {action.description}
                            </h4>
                            <div className="bg-gray-800/50 p-3 rounded border border-gray-700">
                              <pre className="text-xs text-gray-300 whitespace-pre-wrap max-h-64 overflow-y-auto">
                                {action.result}
                              </pre>
                            </div>
                            <p className="text-xs text-gray-600 mt-2">
                              {new Date(action.timestamp).toLocaleString()}
                            </p>
                          </div>
                        </div>
                      </CardContent>
                    </Card>
                  ))}

                  {/* Final Result */}
                  {activeTask.result && (
                    <Card className="bg-green-600/10 border-green-600/30">
                      <CardHeader>
                        <CardTitle className="text-green-400 flex items-center gap-2">
                          <CheckCircle className="w-5 h-5" />
                          Final Result
                        </CardTitle>
                      </CardHeader>
                      <CardContent>
                        <div className="bg-gray-900/50 p-4 rounded border border-gray-700 mb-3">
                          <pre className="text-sm text-gray-100 whitespace-pre-wrap">
                            {activeTask.result}
                          </pre>
                        </div>
                        <Button
                          onClick={() => copyResult(activeTask.result!)}
                          className="bg-green-600 hover:bg-green-700"
                        >
                          <Copy className="w-4 h-4 mr-2" />
                          Copy Result
                        </Button>
                      </CardContent>
                    </Card>
                  )}
                </div>
              ) : (
                <div className="flex items-center justify-center h-full min-h-[500px]">
                  <div className="text-center max-w-md">
                    <Bot className="w-20 h-20 mx-auto mb-6 text-green-400" />
                    <h2 className="text-2xl font-semibold text-green-400 mb-3">
                      Ollama Agent System
                    </h2>
                    <p className="text-gray-400 mb-6">
                      Deploy autonomous agents powered by local LLM for research, analysis, and task execution
                    </p>
                  </div>
                </div>
              )}
            </div>
          </ScrollArea>
        </div>

        {/* Deploy Interface */}
        <div className="border-t border-green-600/30 p-4 bg-gray-950">
          <div className="space-y-3">
            <Textarea
              value={inputGoal}
              onChange={(e) => setInputGoal(e.target.value)}
              placeholder="Enter agent goal: 'Research the latest trends in artificial intelligence' or 'Analyze the benefits of renewable energy'"
              className="min-h-[60px] bg-gray-900 border-gray-700 text-white"
              disabled={isRunning}
            />
            <div className="flex gap-3">
              <Button
                onClick={deployAgent}
                disabled={isRunning || !inputGoal.trim()}
                className="bg-green-600 hover:bg-green-700 flex-1"
              >
                {isRunning ? (
                  <>
                    <Brain className="w-4 h-4 animate-pulse mr-2" />
                    WORKING...
                  </>
                ) : (
                  <>
                    <Play className="w-4 h-4 mr-2" />
                    DEPLOY AGENT
                  </>
                )}
              </Button>
              <Button onClick={testConnection} variant="outline" className="border-green-600">
                Test Connection
              </Button>
            </div>
          </div>
        </div>
      </div>

      {/* Settings */}
      <div className="w-72 border-l border-green-600/30 bg-gray-950 p-4">
        <div className="space-y-4">
          <div className="flex items-center gap-2 mb-4">
            <Settings className="w-4 h-4" />
            <span className="text-sm font-medium">OLLAMA CONFIG</span>
          </div>

          <div className="space-y-3">
            <div>
              <label className="text-xs text-gray-400 block mb-1">HOST</label>
              <input
                type="text"
                value={ollamaHost}
                onChange={(e) => setOllamaHost(e.target.value)}
                className="w-full px-2 py-1 bg-gray-900 border border-gray-700 rounded text-white text-xs"
                placeholder="http://localhost:11434"
              />
            </div>

            <div>
              <label className="text-xs text-gray-400 block mb-1">MODEL</label>
              <input
                type="text"
                value={model}
                onChange={(e) => setModel(e.target.value)}
                className="w-full px-2 py-1 bg-gray-900 border border-gray-700 rounded text-white text-xs"
                placeholder="llama3.2:3b"
              />
            </div>
          </div>

          <div className="pt-4 border-t border-gray-800">
            <h4 className="text-xs font-medium mb-2">STATUS</h4>
            <div className="space-y-1 text-xs text-gray-400">
              <div className="flex justify-between">
                <span>Active:</span>
                <span>{isRunning ? 'Yes' : 'No'}</span>
              </div>
              <div className="flex justify-between">
                <span>Tasks:</span>
                <span>{tasks.length}</span>
              </div>
              <div className="flex justify-between">
                <span>Completed:</span>
                <span className="text-green-400">{tasks.filter(t => t.status === 'completed').length}</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default OllamaAgentSystem;